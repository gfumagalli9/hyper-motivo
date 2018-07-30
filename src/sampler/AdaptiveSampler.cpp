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
#include <google/dense_hash_set>
#include "../common/common.h"
#include "ColorCodingSpanningTreeCounter.h"

constexpr unsigned int AdaptiveSampler::suffSamples;


AdaptiveSampler::AdaptiveSampler(UndirectedGraph* g, std::string dtzFile, std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> *tc, unsigned int size, TreeletTableCollection* ttc)
{
	this->g = g;
	this->size = size;
	this->totSamples = 0;
	this->ttc = ttc;

	// copy, or read, the treelet counts
	if (tc)
	{
		for(const auto &it : *tc)
			numTreelets[it.first] = it.second;
	}
	else
    {
		CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias> reader(dtzFile);
		TreeletTable table(&reader);
		std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> counts;
		for(UndirectedGraph::vertex_t u = 0; u < table.number_of_vertices(); u++)
			for(TreeletTable::const_iterator it = table.begin(u); !it.is_over(); ++it)
				numTreelets[it.treelet()] += it.count(); //FIXME: Replace std::map ?
	}

	// init the residuals and the priorities -- the most frequent treelet comes first
	for (const auto &it : numTreelets)
		treeletPriority.insert(it.first, 100.0 + 1.0 * numTreelets[it.first] / totTreelets);
	updateSampler();
}


/**
 * Single-threaded adaptive sampling.
 * At return time, count_table maps each graphlet H found to a pair<int, double>(c, w)
 * where c is the number of times H occurred in the sampling and w is a certain weight.
 * In expectation, the ratio c/w equals the total number of occurrence of H in the graph.
 */
void AdaptiveSampler::sample_st(int num_samples, std::map<Occurrence, std::pair<int, double>, OcurrenceFootprintLess>* count_table, Random* rng)
{
	Occurrence h;

	double totTreelets = 0;
	for(auto it : numTreelets)
		totTreelets += it.second;

	for(uint64_t i = 0; i < num_samples; i++)
	{
        sampler->sample_one(&h, rng);
		graphletCount[h]++;

        //Update weights
		for(const auto &it : graphletCount)
			graphletWeight[it.first] += spTreeCounter.num_spanning_trees(it.first, currentTreelet) / static_cast<double>(numTreelets[currentTreelet]);

		if(graphletCount[h] == suffSamples) // Did we reach the threshold?
		{
		    //FIXME: Does this output belong here?
			std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
			std::cout << "graphlet " << h.text_footprint() << " done" << std::endl;

			completedGraphlets.insert(h);
			for (auto it : numTreelets)
			{
				double r = 0;
				for (const Occurrence &j : completedGraphlets)
					r += (static_cast<double>(graphletCount[j]) * spTreeCounter.num_spanning_trees(j, it.first)) / (it.second * graphletWeight[j]);

				r = std::max(0.0, 1.0 - r);
				r = std::round(r * 100) + numTreelets[it.first] / totTreelets;
				treeletPriority.insert(it.first, r);
			}
			if (!(currentTreelet == treeletPriority.last_key()))  //FIXME?
				updateSampler();

			std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
			totManagementTime += delta_t.count();
		}
	}
	for (const auto it : graphletCount)
        (*count_table)[it.first] = std::pair<int, double>(it.second, graphletWeight[it.first]);
}

/**
 * Pick the most efficient treelet and rebuild the underlying sampler.
 */
void AdaptiveSampler::updateSampler()
{
	totTreeletSwitches++;
	currentTreelet = treeletPriority.last_key();
	delete treeletSelector;
	treeletSelector = new TreeletSelector(TreeletSelector::MODE_INCLUDE, size);
	treeletSelector->add_treelet(currentTreelet, false);
	delete sampler;
	sampler = new OccurrenceSampler(g, ttc, size, false, true, true, true);
	sampler->set_selector(treeletSelector, 1); //FIXME: Number of threads
	std::cout << "using treelet " << currentTreelet.get_structure() << std::endl;
}

/**
 * Single-threaded (non-adaptive) sampling.
 * At return time, count_table maps each graphlet H found to a pair<int, double>(c, w)
 * where c is the number of times H occurred in the sampling and w is a certain weight.
 * In expectation, the ratio c/w equals the total number of occurrence of H in the graph.
 * This methods does *not* switch currentTreelet along the sampling.
 */
void AdaptiveSampler::do_sample_mt(int num_samples, std::map<Occurrence, int, OcurrenceFootprintLess> *counts, Random *rng)
{
	Occurrence h;
	for(uint64_t i = 0; i < num_samples; i++)
	{
        sampler->sample_one(&h, rng);
		(*counts)[h]++;
	}

	delete rng;
}

/**
 * Multi-threaded adaptive sampling.
 */
//FIXME: Return type. We are returning a copy
SampleTable AdaptiveSampler::sample(unsigned int n_samples, unsigned int number_of_threads, Random* rng)
{
	SampleTable table;
	if (n_samples == 0)
		return table;

	std::map<Occurrence, std::pair<int, double>, OcurrenceFootprintLess> occTab;
	std::map<Occurrence, std::pair<int, double>, OcurrenceFootprintLess> oldOccTab;
	if (number_of_threads == 1)
		sample_st(n_samples, &occTab, rng);
	else
    {
        //FIXME: int looks suspicious.. is it a graphlet count? Use TreeletTable:treelet_count_t instead
		// these tables are one per thread, and hold the graphlet counts
		auto* count_tabs = new std::map<Occurrence, int, OcurrenceFootprintLess>[number_of_threads];
		auto worker_threads = new std::thread[number_of_threads];
		unsigned int samples_rem = n_samples;

		google::dense_hash_set<Occurrence, OccurrenceFootprintHash, OccurrenceFootprintEquality> seen_now;
		seen_now.set_empty_key(Occurrence());
		google::dense_hash_set<Occurrence, OccurrenceFootprintHash, OccurrenceFootprintEquality> seen;
		seen.set_empty_key(Occurrence());
		google::dense_hash_set<Occurrence, OccurrenceFootprintHash, OccurrenceFootprintEquality> completed_now;
		completed_now.set_empty_key(Occurrence());

		double joinTime = 0, mergeTime = 0, weightsTime = 0, effTime = 0, prioTime = 0, samplerTime = 0;

		// MAIN CYCLE, LAUNCHES THREADS
		while (samples_rem > 0) // take suffSamples more samples, in parallel
		{
			// 1. SET UP AND RUN THREADS
			seen_now.clear();
			completed_now.clear();
			//FIXME: Types
			unsigned int round_samples = std::min(std::max(number_of_threads * 50, suffSamples), samples_rem);
			unsigned int round_samples_rem = round_samples;
//			std::cout << "Taking " << rem_samples_1 << " samples " << std::endl;
			unsigned int thread_samples = std::min((round_samples_rem + number_of_threads - 1) / number_of_threads, round_samples_rem);
			for (unsigned int id = 0; id < number_of_threads; id++)
			{
				thread_samples = std::min(thread_samples, round_samples_rem);
				if (thread_samples == 0)
					break;

				auto ct = &count_tabs[id];
//				std::cout << "Thread " << id << " samples " << nsamples << std::endl;
			    Random *r = rng->derived_rng();
				worker_threads[id] = std::thread([this, thread_samples, ct, r] { do_sample_mt(thread_samples, ct, r);}); //FIXME: One random for each thread
				round_samples_rem -= thread_samples;
				samples_rem -= thread_samples;
			}

			treeletSamples[currentTreelet] += round_samples;
//			std::cout << "Joining " << worker_threads.size() << " threads " << std::endl;

            //FIXME: What is joinTime supposed to be?
            //It can be anything between 0 and the length of the time interval between the termination time of the first and last thread
            worker_threads[0].join();
			std::chrono::time_point < std::chrono::steady_clock > tstart_join = std::chrono::steady_clock::now();

			for (unsigned int id = 1; id < number_of_threads; id++)
				worker_threads[id].join();

			joinTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_join)).count();


			// 2. MERGE COUNTS
//			std::cout << "Done, merging counts..." << std::endl;
			std::chrono::time_point < std::chrono::steady_clock > tstart_merge =
					std::chrono::steady_clock::now();
			bool recomputeTreelet = false;
			for (int id = 0; id<number_of_threads; id++)
			{
//				std::cout << "Thread " << id << " found " << count_tabs[id].size() << " graphlets"
//						<< std::endl;
				for (auto it : count_tabs[id])
				{
					Occurrence o = it.first;
					seen_now.insert(o);
					seen.insert(o);
					int cnt = it.second;
					if (occTab[o].first < suffSamples && occTab[o].first + cnt >= suffSamples) {
						completedGraphlets.insert(o);
						completed_now.insert(o);
						std::cout << "graphlet " << o.text_footprint() << " done" << std::endl;
						recomputeTreelet = true;
					}
					occTab[o].first += cnt;
//					std::cout << o.text_footprint() << ":" << occTab[o].first << std::endl;
				}
				count_tabs[id].clear();
			}

			mergeTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_merge)).count();

			// UPDATE WEIGHTS: occTab[o].second will hold the correct weight w[o]
//			std::cout << "Updating weights... " << std::endl;
			std::chrono::time_point < std::chrono::steady_clock > tstart_w =
					std::chrono::steady_clock::now();
			for (Occurrence j : seen) {
				occTab[j].second = 0;
				CachedSTC::treelet_table_t* spanTable = spTreeCounter.get_t_table(j);
				for (auto itr_i : *(spanTable))
					occTab[j].second += treeletSamples[itr_i.first] * itr_i.second * 1.0
							/ numTreelets[itr_i.first];
			}
			weightsTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_w)).count();

			// recompute treelet efficiencies
			if (recomputeTreelet) {
//				std::cout << "Recomputing treelet priorities..." << std::endl;
				std::chrono::time_point < std::chrono::steady_clock > tstart =
						std::chrono::steady_clock::now();
				google::dense_hash_set<Treelet, Treelet::TreeletHash, Treelet::compare_eq> touchedTreelets;
				touchedTreelets.set_empty_key(Treelet::invalid_treelet);

				// 1. subtract the frequency of completed graphlets
				treeletInefficiency.clear();
				for (Occurrence j : completedGraphlets) {
					CachedSTC::treelet_table_t* spanTable = spTreeCounter.get_t_table(j);
					int cj = occTab[j].first;
					double wj = occTab[j].second;
					for (auto t_itr : (*spanTable)) {
						Treelet i = t_itr.first;
						touchedTreelets.insert(i);
						treeletInefficiency[i] += (1.0 * (*spanTable)[i] * cj)
								/ (wj * numTreelets[i]);
					}
				}
				effTime +=
						(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
								- tstart)).count();

				// 2. recompute inefficiencies for the affected treelets
				std::chrono::time_point < std::chrono::steady_clock > tstart1 =
						std::chrono::steady_clock::now();
				for (Treelet i : touchedTreelets) {
					treeletInefficiency[i] = std::min(1.0, treeletInefficiency[i]);
					// the actual priority is a mix of efficiency and abundance
					double prio = std::round(100 * (1.0 - treeletInefficiency[i]))
							+ 1.0 * numTreelets[i] / totTreelets;
					treeletPriority.insert(i, prio);
				}
				prioTime +=
						(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
								- tstart1)).count();

				// Do we need to choose a new treelet?
				std::chrono::time_point < std::chrono::steady_clock > tstart2 =
						std::chrono::steady_clock::now();
				if (!(currentTreelet == treeletPriority.last_key()))
					updateSampler();
				samplerTime +=
						(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
								- tstart2)).count();

				//				std::cout << treeletPriority << std::endl;
				totManagementTime +=
						(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
								- tstart)).count();
			}
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
	}
	double p = pcol(size, size);
	for (const auto &it : occTab)
	{
		SampleTable::Entry e;
		//e.occ = it.first;
		e.fingerprint = (it.first.text_footprint() );
		e.num_spanning_trees = 0;
		e.sample_count = it.second.first;
		e.estimate_graph_occurrences = it.second.first * size / (it.second.second * p);
		table.addEntry(e);
	}
	std::cout << "total management time: " << totManagementTime << std::endl;
	table.estimateFrequencies();
	double norm2 = 0, totSamples = 0;
	const SampleTable::Entry *entries = table.get_entries();
	for (uint64_t  i=0; i<table.size(); i++)
	{
		totSamples += entries[i].sample_count;
		norm2 += entries[i].sample_count * entries[i].sample_count;
	}
	norm2 = std::sqrt(norm2) / totSamples;
	std::cout << "norm-2 of the sample distribution: " << norm2 << std::endl;
	return table;
}

