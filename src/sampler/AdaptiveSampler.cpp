/*
 * AdaptiveSampler.cpp
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#include "AdaptiveSampler.h"
#include "OccurrenceSampler.h"

AdaptiveSampler::AdaptiveSampler(UndirectedGraph* g, std::string dtzFile,
		std::map<Treelet, TreeletTable::treelet_count_t> *tc, unsigned int size, Random* rng,
		TreeletTableCollection* ttc, sampler_opts opts) {
	this->g = g;
	this->size = size;
	this->rng = rng;
	this->totSamples = 0;
	this->ttc = ttc;
	this->opts = opts;
	// copy, or read, the treelet counts
	if (tc) {
		for (auto &it : *tc)
			treeletCounts[it.first] = it.second;
	} else {
		CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
				TreeletTable::may_alias> reader(dtzFile);
		TreeletTable table(&reader);
		std::map<Treelet, TreeletTable::treelet_count_t> counts;
		for (UndirectedGraph::vertex_t u = 0; u < table.number_of_vertices(); u++)
			for (TreeletTable::const_iterator it = table.begin(u); !it.is_over(); ++it)
				treeletCounts[it.treelet()] += it.count();
	}
	// init the residuals -- we will start by picking the most frequent treelet
	TreeletTable::treelet_count_t max_count = 0;
	Treelet max_treelet;
	for (auto &it : treeletCounts) {
		if (it.second > max_count) {
			max_treelet = it.first;
			max_count = it.second;
		}
//		std::cout << it.first.get_structure() << " : " << it.second << std::endl;
		residuals.insert(it.first, 0.0);
//		std::cout << residuals.size() << std::endl;
	}
	residuals.insert(max_treelet, 1.0);
//	std::cout << residuals << std::endl;
	updateSampler();
}

AdaptiveSampler::~AdaptiveSampler() {
}

void AdaptiveSampler::sample_one(Occurrence* occurrence) {
	sampler->sample_one(occurrence);
}

/**
 * Adaptive sampling. The returned table maps each graphlet H found to a pair<int, double>(c, w)
 * where c is the number of times H occurred in the sampling and w is a certain weight.
 * In expectation, the ratio c/w equals the total number of occurrence of H in the graph.
 */
std::map<Occurrence, std::pair<int, double>, Occurrence::OccurrenceCompare> AdaptiveSampler::sample_many(
		int num_samples) {
	Occurrence h;
	OccurrenceCanonicizer canonicizer(size);
	double totTreelets = 0;
	for (auto it : treeletCounts)
		totTreelets += it.second;
	for (uint64_t i = 0; i < num_samples; i++) {
//		std::cout << ".";
		sample_one(&h);
		canonicizer.canonicize(&h);
		counts[h]++;
		for (auto it : counts) {
			Occurrence h1 = it.first;
			weights[h1] += stc.num_spanning_trees(h1, currentTreelet) * 1.0
					/ treeletCounts[currentTreelet];
			//			std::cout << "spanning(" << h1.text_footprint() << "," << currentTreelet.get_structure()
			//					<< ") = " << stc.num_spanning_trees(h1, currentTreelet) << std::endl;
		}
		if (counts[h] == c) { // reached the threshold
			std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
			std::cout << "graphlet " << h.text_footprint() << " done" << std::endl;
			S.insert(h);
			for (auto it : treeletCounts) {
				Treelet i = it.first;
				double r = 0;
				for (Occurrence j : S)
					r += (1.0 * counts[j] * stc.num_spanning_trees(j, i))
							/ (it.second * weights[j]);
				r = std::max(0.0, 1.0 - r);
				r = std::round(r * 100) + treeletCounts[i] / totTreelets;
				residuals.insert(i, r);
//				std::cout << "residual[" << it.first.get_structure() << "] : "
//						<< residuals.get(it.first) << std::endl;
			}
//			std::cout << residuals << std::endl;
			if (!(currentTreelet == residuals.last_key())) {
				updateSampler();
			}
			std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
			totUpdateTime += delta_t.count();
		}
	}
	std::map<Occurrence, std::pair<int, double>, Occurrence::OccurrenceCompare> count_table;
	for (auto it : counts) {
		Occurrence o = it.first;
		std::pair<int, double> p(it.second, weights[it.first]);
		count_table[o] = p;
	}
	return count_table;
}

/**
 * Pick the most efficient treelet and rebuild the underlying sampler.
 */
void AdaptiveSampler::updateSampler() {
	currentTreelet = residuals.last_key();
	delete treeletSelector;
	treeletSelector = new TreeletSelector(TreeletSelector::MODE_INCLUDE, size);
	treeletSelector->add_treelet(currentTreelet, false);
	delete sampler;
	sampler = new OccurrenceSampler(g, ttc, size, rng, opts.vertices, opts.graphlets,
			opts.spanning_trees, opts.footprints, opts.canonicize, opts.norejection, opts.text,
			opts.group, nullptr, opts.threads, treeletSelector, opts.tot_treelets,
			opts.store_only_0);
	std::cout << "using treelet " << currentTreelet.get_structure() << std::endl;
}

/**
 * Return a SampleTable with the estimated frequencies, occurrences, etc
 */
SampleTable AdaptiveSampler::sample(int n_samples, int number_of_threads) {
	SampleTable table;
	std::map<Occurrence, std::pair<int, double>, Occurrence::OccurrenceCompare> occTab =
			sample_many(n_samples);
	double p = pcol(size, size);
	for (auto it : occTab) {
		SampleTable::Entry e;
		e.occ = it.first;
		e.fingerprint = it.first.text_footprint();
		e.num_spanning_trees = 0;
		e.sample_count = it.second.first;
		e.estimate_graph_occurrences = it.second.first * size / (it.second.second * p);
		table.addEntry(e);
	}
	table.estimateFrequencies();
	return table;
}
