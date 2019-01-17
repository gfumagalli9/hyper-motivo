/*
 * AdaptiveSampler.cpp
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#include "AdaptiveSampler.h"
#include "OccurrenceSampler.h"
#include <thread>
#include <vector>
#include <queue>
#include <cmath>
#include "../common/util.h"
#include "ColorCodingSpanningTreeCounter.h"

AdaptiveSampler::AdaptiveSampler(UndirectedGraph* graph, unsigned int size, TreeletTableCollection* ttc, bool store_only_on_0)
			: graph(graph), size(size), ttc(ttc), store_only_on_0(store_only_on_0)
{
	std::map<Treelet, TreeletTable::treelet_count_t> numTreelets2;
	this->treeletSamples.set_empty_key(invalid_treelet);

	// Read the treelet counts
	TreeletTable *table = ttc->get_table(size);
	for (UndirectedGraph::vertex_t u = 0; u < table->number_of_vertices(); u++)
		for (TreeletTable::const_iterator it = table->begin(u); !it.is_over(); ++it)
			numTreelets2[it.treelet()] += it.count(); //FIXME: Replace std::map ?


	treeletClassMap.set_empty_key(invalid_treelet);
	for (const auto &[treelet, count] : numTreelets2) // cumulate each treelet's count to its representant's count
	{
	    //Try to make "treelet" its own representant
	    auto [representant_it, inserted] = treeletToRepresentant.insert( std::make_pair(treelet, treelet) );
		if (inserted) //If the insertion was successful
		{
            // "treelet" will be the representant of its class
			treeletClassMap[treelet] = TreeletClass(treelet); //FIXME: avoid copy
			for (const Treelet &t : treeletClassMap[treelet].get_all()) //FIXME: avoid searching
				treeletToRepresentant[t] = treelet;
		}

		numTreelets[representant_it->second] += count; //put the count on the representant
	}

	// init the residuals and the priorities -- the most frequent treelet comes first
	for (const auto &[treelet, count] : numTreelets)
    {
        totTreelets += count;
        treeletPriority.insert(treelet, 100.0 + 1.0 * numTreelets[treelet] / totTreelets);
    }

	update_sampler();
}

/**
 * Pick the most efficient treelet(s) and rebuild the underlying sampler.
 */
void AdaptiveSampler::update_sampler()
{
	totTreeletSwitches++;

	delete treeletSelector;
	treeletSelector = new TreeletSelector(TreeletSelector::MODE_INCLUDE, size);
	currentTreelet = treeletPriority.last_key();

	TreeletClass tc(currentTreelet); //FIXME: Do we need TreeletClass?
	for (const Treelet &t : tc.get_all())
		treeletSelector->add_treelet(t);

	delete sampler;
	sampler = new OccurrenceSampler(graph, ttc, size, false, true, true, true);
	sampler->set_selector(treeletSelector, 1); //FIXME: Number of threads
}

void AdaptiveSampler::recomputeTreeletPriorities(occ_pair_table_t& occTab)
{
	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();

	// 1. subtract the frequency of completed graphlets
	std::map<Treelet, double> treeletInefficiency; // estimated probability of yielding a graphlet in completedGraphlets
	for (const Occurrence &j : completedGraphlets)
	{
		const auto& occ_info = occTab[j];
		CachedSTC::treelet_table_t* spanTable = spTreeCounter.get_t_table(j);
		for (const auto &[spanning_treelet, noccurences]: (*spanTable))
		{
			const auto num_it = numTreelets.find(spanning_treelet);
			if (num_it != numTreelets.cend()) //If the test succeeds, it.first is a necessarily a representant
				treeletInefficiency[spanning_treelet] += (1.0 * noccurences * occ_info.num_occurrences) / (occ_info.weight * num_it->second);
		}
	}
	effTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();


	// 2. recompute inefficiencies for the affected treelets
	std::chrono::time_point < std::chrono::steady_clock > tstart1 = std::chrono::steady_clock::now();
	for(const auto &[treelet, ineff] : treeletInefficiency)
	{
		double prio = ((ineff<=1)?std::round(100 * (1.0 - ineff) ):0) + 1.0 * numTreelets[treelet] / totTreelets;
		treeletPriority.insert(treelet, prio);
	}
	prioTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart1)).count();

	// Do we need to choose a new treelet?
	std::chrono::time_point < std::chrono::steady_clock > tstart2 = std::chrono::steady_clock::now();
	if (currentTreelet != treeletPriority.last_key())
		update_sampler();

	samplerTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart2)).count();

	totManagementTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();
}

/**
 * Single-threaded (non-adaptive) sampling.
 * At return time, count_table maps each graphlet H found to a pair<int, double>(c, w)
 * where c is the number of times H occurred in the sampling and w is a certain weight.
 * In expectation, the ratio c/w equals the total number of occurrence of H in the graph.
 * This methods does *not* switch currentTreelet along the sampling.
 */
void AdaptiveSampler::do_sample_mt(const uint64_t num_samples, occ_count_table_t *counts, Random *rng, CachedSTC *stc)
{
	Occurrence h;
	for (uint64_t i = 0; i < num_samples; i++)
	{
		sampler->sample_one(&h, rng);
		stc->update_tables(h);
		(*counts)[h]++;
	}

	delete rng;
}

/**
 * Multi-threaded adaptive sampling.
 */
SampleTable* AdaptiveSampler::sample(uint64_t n_samples, unsigned int number_of_threads, Random* rng, double time_budget)
{
	auto table = new SampleTable();
	if (n_samples == 0 && (time_budget < 0 || std::isinf(time_budget) ))
		return table;

	occ_pair_table_t occTab;
	occTab.set_empty_key(Occurrence());
	//FIXME: int looks suspicious.. is it a graphlet count? Use TreeletTable:treelet_count_t instead
	// these tables are one per thread, and hold the graphlet counts
	occ_count_table_t* count_tabs = new occ_count_table_t[number_of_threads];
	for (unsigned int id = 0; id < number_of_threads; id++)
		count_tabs[id].set_empty_key(Occurrence());
	uint64_t samples_rem = n_samples;

	occ_set_t seen_now;
	seen_now.set_empty_key(Occurrence());
	occ_set_t seen;
	seen.set_empty_key(Occurrence());
	occ_set_t completed_now;
	completed_now.set_empty_key(Occurrence());

	std::chrono::time_point < std::chrono::steady_clock > totTimeStart = std::chrono::steady_clock::now();

	// MAIN CYCLE, LAUNCHES THREADS
	while ((samples_rem > 0 || n_samples == 0) && totTime < time_budget) // take suffSamples more samples, in parallel
	{
		std::queue<std::thread> thread_q; //FIXME: there is no need for a queue
//		std::cout << std::endl << "new sample round, treelet priorities are:" << std::endl	<< treeletPriority << std::endl;
		// 1. SET UP AND RUN THREADS
		seen_now.clear();
		completed_now.clear();
		//FIXME: Types
		if (n_samples == 0)
			samples_rem = (uint64_t) std::max(number_of_threads * 50, suffSamples);
		uint64_t round_samples = std::min((uint64_t) std::max(number_of_threads * 50, suffSamples),
				samples_rem);
		uint64_t round_samples_rem = round_samples;
		uint64_t thread_samples = std::ceil(1.0 * round_samples_rem / number_of_threads);
		CachedSTC* stc = &spTreeCounter;
		for(unsigned int id=0; id < number_of_threads && round_samples_rem > 0; id++)
		{
			thread_samples = std::min(thread_samples, round_samples_rem);
			auto ct = &count_tabs[id];
			Random *r = rng->derived_rng();
			thread_q.push( std::thread( [this, thread_samples, ct, r, stc] {do_sample_mt(thread_samples, ct, r, stc);}));
			round_samples_rem -= thread_samples;
			samples_rem -= thread_samples;
		}

		treeletSamples[currentTreelet] += round_samples;

		//FIXME: What is joinTime supposed to be?
		//It can be anything between 0 and the length of the time interval between the termination time of the first and last thread
		std::chrono::time_point < std::chrono::steady_clock > tstart_join =
				std::chrono::steady_clock::now();
//			std::cout << "joining threads..." << std::endl;
		while (!thread_q.empty()) {
			thread_q.front().join();
			thread_q.pop();
		}
		joinTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_join)).count();

		// 2. MERGE COUNTS
		std::chrono::time_point < std::chrono::steady_clock > tstart_merge = std::chrono::steady_clock::now();
		bool recomputeTreelet = false;
		for (unsigned int id = 0; id < number_of_threads; id++)
		{
			for (auto it : count_tabs[id])
			{
				Occurrence o = it.first;
				seen_now.insert(o);
				seen.insert(o);
				int cnt = it.second;
				if (occTab[o].num_occurrences < suffSamples && occTab[o].num_occurrences + cnt >= suffSamples) {
					completedGraphlets.insert(o);
					completed_now.insert(o);
					recomputeTreelet = true;
				}
				occTab[o].num_occurrences += cnt;
			}
			count_tabs[id].clear();
		}

		mergeTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_merge)).count();

		// 3. UPDATE WEIGHTS: occTab[o].second will hold the correct weight w[o]
		std::chrono::time_point < std::chrono::steady_clock > tstart_w = std::chrono::steady_clock::now();
		for (const Treelet & t : treeletClassMap[currentTreelet].get_all())
			for (const auto &it : *(spTreeCounter.get_reverse_table(t)))
				occTab[it.first].weight += round_samples * it.second * 1.0 / numTreelets[currentTreelet];


		weightsTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_w)).count();

		if (recomputeTreelet)
			recomputeTreeletPriorities(occTab);

		totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - totTimeStart)).count();
		if (totTime >= time_budget)
			break;
	}

	delete[] count_tabs;
	std::cout << "time spent in joining threads: " << joinTime << std::endl;
	std::cout << "time spent in count merge: " << mergeTime << std::endl;
	std::cout << "time spent in weights update: " << weightsTime << std::endl;
	std::cout << "time spent in sampler update: " << samplerTime << std::endl;
	std::cout << "time spent in computing efficiencies: " << effTime << std::endl;
	std::cout << "time spent in updating priorities: " << prioTime << std::endl;
	std::cout << "time spent on spanning trees: " << spTreeCounter.running_time() << std::endl;
	std::cout << "total treelet switches: " << totTreeletSwitches << std::endl;

	double p = pcol(size, size);
	for (const auto &it : occTab)
	{
		SampleTable::Entry e;
		//e.occ = it.first;
		e.fingerprint = (it.first.text_footprint());
		e.num_spanning_trees = 0;
		e.sample_count = it.second.num_occurrences;
		e.estimate_graph_occurrences = it.second.num_occurrences * (store_only_on_0 ? size : 1) / (it.second.weight * p);
		table->addEntry(e);
	}
	std::cout << "total management time: " << totManagementTime << std::endl;
	table->estimateFrequencies();
	double norm2 = 0, totSamples = 0;
	const SampleTable::Entry *entries = table->get_entries();
	for (uint64_t i = 0; i < table->size(); i++) {
		totSamples += entries[i].sample_count;
		norm2 += entries[i].sample_count * entries[i].sample_count;
	}
	norm2 = std::sqrt(norm2) / totSamples;
	std::cout << "norm-2 of the sample distribution: " << norm2 << std::endl;
	return table;
}

