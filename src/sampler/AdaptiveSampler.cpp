/*
 * AdaptiveSampler.cpp
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#include "AdaptiveSampler.h"
#include "OccurrenceSampler.h"
#include "../common/graph/SimpleGraph.h"
#include <thread>
#include <vector>
#include <google/dense_hash_set>
#include "../common/common.h"
#include "ColorCodingSpanningTreeCounter.h"
#include <queue>

constexpr unsigned int AdaptiveSampler::suffSamples;

AdaptiveSampler::AdaptiveSampler(UndirectedGraph* g, std::string dtzFile,
		std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> *tc,
		unsigned int size, TreeletTableCollection* ttc, bool store_only_on_0) {
	this->g = g;
	this->size = size;
	this->totSamples = 0;
	this->ttc = ttc;
	this->store_only_on_0 = store_only_on_0;
	std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> numTreelets2;
	this->treeletSamples.set_empty_key(Treelet::invalid_treelet);

	// copy, or read, the treelet counts
	if (tc) {
		for (const auto &it : *tc)
			numTreelets2[it.first] = it.second;
	} else {
		CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
				TreeletTable::may_alias> reader(dtzFile);
		TreeletTable table(&reader);
		for (UndirectedGraph::vertex_t u = 0; u < table.number_of_vertices(); u++)
			for (TreeletTable::const_iterator it = table.begin(u); !it.is_over(); ++it)
				numTreelets2[it.treelet()] += it.count(); //FIXME: Replace std::map ?
	}

	treeletClassMap.set_empty_key(Treelet::invalid_treelet);
	for (const auto &it : numTreelets2) { // cumulate each treelet's count to its representant's count
		if (!treeletToRepresentant.count(it.first)) { // this treelet will be the representant of its class
			treeletClassMap[it.first] = TreeletClass(it.first);
			for (const Treelet &t : treeletClassMap[it.first].get_all())
				treeletToRepresentant[t] = it.first;
		}
		numTreelets[treeletToRepresentant[it.first]] += numTreelets2[it.first]; // put the count on the representant
	}

	// init the residuals and the priorities -- the most frequent treelet comes first
	for (const auto &it : numTreelets)
		totTreelets += it.second;
	for (const auto &it : numTreelets)
		treeletPriority.insert(it.first, 100.0 + 1.0 * numTreelets[it.first] / totTreelets);
//	std::cout << treeletPriority << std::endl;
	updateSampler();
}

/**
 * Pick the most efficient treelet(s) and rebuild the underlying sampler.
 */
void AdaptiveSampler::updateSampler() {
	totTreeletSwitches++;
	delete treeletSelector;
	treeletSelector = new TreeletSelector(TreeletSelector::MODE_INCLUDE, size);
	currentTreelet = treeletPriority.last_key();
	TreeletClass tc(currentTreelet);
	for (const Treelet &t : tc.get_all())
		treeletSelector->add_treelet(t, false);
	delete sampler;
	sampler = new OccurrenceSampler(g, ttc, size, false, true, true, true);
	sampler->set_selector(treeletSelector, 1); //FIXME: Number of threads
//	std::cout << "using treelet " << currentTreelet.get_structure() << std::endl;
}

/**
 * Single-threaded (non-adaptive) sampling.
 * At return time, count_table maps each graphlet H found to a pair<int, double>(c, w)
 * where c is the number of times H occurred in the sampling and w is a certain weight.
 * In expectation, the ratio c/w equals the total number of occurrence of H in the graph.
 * This methods does *not* switch currentTreelet along the sampling.
 */
void AdaptiveSampler::do_sample_mt(int num_samples, occ_count_table_t *counts, Random *rng,
		CachedSTC *stc) {
	Occurrence h;
	for (uint64_t i = 0; i < num_samples; i++) {
		sampler->sample_one(&h, rng);
		stc->update_tables(h);
		(*counts)[h]++;
	}

	delete rng;
}

void AdaptiveSampler::recomputeTreeletPriorities(occ_pair_table_t& occTab) {
	// 4. RECOMPUTE TREELET PRIORITY
	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
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
			if (!numTreelets.count(i)) // treelet spanning j, but excluded from building
				continue;
			Treelet repr = treeletToRepresentant[i];
			touchedTreelets.insert(repr);
			treeletInefficiency[repr] += (1.0 * (*spanTable)[i] * cj) / (wj * numTreelets[repr]);
		}
	}
	effTime +=
			(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();

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
	prioTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
			- tstart1)).count();

	// Do we need to choose a new treelet?
	std::chrono::time_point < std::chrono::steady_clock > tstart2 =
			std::chrono::steady_clock::now();
	if (!(currentTreelet == treeletPriority.last_key()))
		updateSampler();
	samplerTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
			- tstart2)).count();

	//				std::cout << treeletPriority << std::endl;
	totManagementTime +=
			(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();
}

/**
 * Multi-threaded adaptive sampling.
 */
SampleTable* AdaptiveSampler::sample(uint64_t n_samples, unsigned int number_of_threads,
		Random* rng, double time_budget) {

	SampleTable* table = new SampleTable();
	if (n_samples == 0
			&& (time_budget < 0 || time_budget == std::numeric_limits<double>::infinity()))
		return table;

	occ_pair_table_t occTab;
	occTab.set_empty_key(Occurrence());
	//FIXME: int looks suspicious.. is it a graphlet count? Use TreeletTable:treelet_count_t instead
	// these tables are one per thread, and hold the graphlet counts
	occ_count_table_t* count_tabs = new occ_count_table_t[number_of_threads];
	for (int id = 0; id < number_of_threads; id++)
		count_tabs[id].set_empty_key(Occurrence());
	uint64_t samples_rem = n_samples;

	occ_set_t seen_now;
	seen_now.set_empty_key(Occurrence());
	occ_set_t seen;
	seen.set_empty_key(Occurrence());
	occ_set_t completed_now;
	completed_now.set_empty_key(Occurrence());

	std::chrono::time_point < std::chrono::steady_clock > totTimeStart =
			std::chrono::steady_clock::now();

	// MAIN CYCLE, LAUNCHES THREADS
	while ((samples_rem > 0 || n_samples == 0) && totTime < time_budget) // take suffSamples more samples, in parallel
	{
		std::queue<std::thread> thread_q;
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
//			std::cout << "Taking " << round_samples_rem << " samples " << std::endl;
		uint64_t thread_samples = std::ceil(1.0 * round_samples_rem / number_of_threads);
		int id = 0;
		CachedSTC* stc = &spTreeCounter;
		while (id < number_of_threads && round_samples_rem > 0) {
			thread_samples = std::min(thread_samples, round_samples_rem);
			auto ct = &count_tabs[id];
//				std::cout << "Thread " << id << " samples " << thread_samples << std::endl;
			Random *r = rng->derived_rng();
			thread_q.push(
					std::thread(
							[this, thread_samples, ct, r, stc] {do_sample_mt(thread_samples, ct, r, stc);})); //FIXME: One random for each thread
			round_samples_rem -= thread_samples;
			samples_rem -= thread_samples;
			id++;
		}
//			std::cout << "samples_rem = " << samples_rem << std::endl;
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
		joinTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
				- tstart_join)).count();

		// 2. MERGE COUNTS
//			std::cout << "Done, merging counts..." << std::endl;
		std::chrono::time_point < std::chrono::steady_clock > tstart_merge =
				std::chrono::steady_clock::now();
		bool recomputeTreelet = false;
		for (int id = 0; id < number_of_threads; id++) {
			for (auto it : count_tabs[id]) {
				Occurrence o = it.first;
				seen_now.insert(o);
				seen.insert(o);
				int cnt = it.second;
				if (occTab[o].first < suffSamples && occTab[o].first + cnt >= suffSamples) {
					completedGraphlets.insert(o);
					completed_now.insert(o);
//					std::cout << "graphlet " << o.text_footprint() << " done" << std::endl;
					recomputeTreelet = true;
				}
				occTab[o].first += cnt;
//					std::cout << o.text_footprint() << ":" << occTab[o].first << std::endl;
			}
			count_tabs[id].clear();
		}

		mergeTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
				- tstart_merge)).count();

		// 3. UPDATE WEIGHTS: occTab[o].second will hold the correct weight w[o]
//		std::cout << "Updating graphlet weights... " << std::endl;
		std::chrono::time_point < std::chrono::steady_clock > tstart_w =
				std::chrono::steady_clock::now();
		for (const Treelet & t : treeletClassMap[currentTreelet].get_all()) {
			for (const std::pair<Occurrence, uint64_t> &it : *(spTreeCounter.get_reverse_table(t))) {
				occTab[it.first].second += round_samples * it.second * 1.0
						/ numTreelets[currentTreelet];
			}
		}

		weightsTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
				- tstart_w)).count();

		if (recomputeTreelet) {
			recomputeTreeletPriorities(occTab);
		}

		totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
				- totTimeStart)).count();
		if (totTime >= time_budget)
			break;
		// std::cout << "totTime = " << totTime << std::endl;
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
	for (const auto &it : occTab) {
		SampleTable::Entry e;
		//e.occ = it.first;
		e.fingerprint = (it.first.text_footprint());
		e.num_spanning_trees = 0;
		e.sample_count = it.second.first;
		e.estimate_graph_occurrences = it.second.first * (store_only_on_0 ? size : 1)
				/ (it.second.second * p);
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

