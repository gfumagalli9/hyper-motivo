/*
 * SampleTable.cpp
 *
 *  Created on: 30 mag 2018
 *      Author: brix
 */

#include "SampleTable.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

SampleTable::SampleTable() {
}

/**
 * Take an (Occurrence, count) table and calculates spanning trees, estimates frequencies, etc
 */
SampleTable::SampleTable(table_t* t, TreeletSelector* ts) {
	if (t->size() == 0)
		return;

	table_t::const_iterator it = t->begin();
	int k = ((Occurrence) t->begin()->first).get_size(); // graphlet size
	// Let's check if the TreeletSelector is excluding just k-stars...
	bool exclude_only_stars = ts && ts->get_treelet_size() == k
			&& ts->get_mode() == TreeletSelector::MODE_EXCLUDE && ts->get_size() == 2;
	if (exclude_only_stars)
		for (int i = 0; i < ts->get_size(); i++)
			exclude_only_stars &= ts->get_treelets()[i].is_star();
	// Let's check if the TreeletSelector is including just k-stars...
	bool include_only_stars = ts && ts->get_treelet_size() == k
			&& ts->get_mode() == TreeletSelector::MODE_INCLUDE && ts->get_size() == 2;
	if (include_only_stars)
		for (int i = 0; i < ts->get_size(); i++)
			include_only_stars &= ts->get_treelets()[i].is_star();

	SpanningTreeCounter stc;
	num_samples = 0;
	double normalized_samples = 0;
	// populate the table
	while (it != t->end()) {
		Entry e;
		e.occ = it->first;
		e.fingerprint = std::string(e.occ.text_footprint());
		e.sample_count = it->second;
		if (exclude_only_stars)
			e.num_spanning_trees = stc.num_spanning_trees_nostars(e.occ);
		else if (include_only_stars)
			e.num_spanning_trees = stc.num_spanning_stars(e.occ);
		else
			e.num_spanning_trees = stc.num_spanning_trees(e.occ, ts);
		num_samples += e.sample_count;
		normalized_samples += (double) e.sample_count / e.num_spanning_trees;
		entries.push_back(e);
		it++;
	}
	// compute the estimate graph frequency
	for (Entry &e : entries) {
		e.estimate_graph_frequency = (double) e.sample_count
				/ (e.num_spanning_trees * normalized_samples);
	}
}

void SampleTable::addEntry(SampleTable::Entry e) {
	entries.push_back(e);
	num_samples += e.sample_count;
}

/**
 * Estimate the total number of occurrences in the graph
 * num_graph_treelets is (an estimate of) the total number of treelets in the graph (not only the colorful ones)
 */
void SampleTable::estimateOccurrences(double num_graph_treelets, bool store_only_0) {
	for (Entry &e : entries) {
		int k = e.occ.get_size();
		e.estimate_graph_occurrences = (1.0 * e.sample_count / num_samples)
				* (1.0 * num_graph_treelets / (e.num_spanning_trees * (store_only_0 ? 1 : k)));

	}
}

/**
 * Returns the table's header.
 */
std::string SampleTable::header() {
	return std::string("motif,sample_count,spanning_trees,estim_freq,estim_occur");
}

SampleTable::~SampleTable() {
// TODO Auto-generated destructor stub
}

/**
 * Merge two tables.
 * In any case, the resulting table has e.sample_count as the sum of the corresponding
 * entries in t1 and t2.
 * The entries in the two tables may have a different number of spanning trees.
 * If this is the case, then in the merged table:
 * 	e.num_spanning_trees = -1
 *	e.estimate_graph_frequency is obtained as an appropriate average of the two tables
 *	e.estimate_graph_occurrences is obtained as an appropriate average of the two tables
 *
 *	tcount1 and tcount2 are the total treelet counts (the number of colorful k-treelets
 *	the sampling was performed on, for t1 and t2 respectively).
 */
SampleTable SampleTable::merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2) {
	SampleTable t;
	std::map<std::string, SampleTable::Entry> merged;
	std::map<std::string, double> weights;
	int s = t.num_samples = t1.get_num_samples() + t2.get_num_samples();
	double p1 = 1.0 * t1.get_num_samples() / s;
	double p2 = 1.0 * t2.get_num_samples() / s;
	for (SampleTable::Entry e : t1.get_entries()) {
		merged[e.fingerprint].sample_count += e.sample_count;
		merged[e.fingerprint].occ = e.occ;
		weights[e.fingerprint] += p1 * e.num_spanning_trees / tcount1;
	}
	for (SampleTable::Entry e : t2.get_entries()) {
		merged[e.fingerprint].sample_count += e.sample_count;
		merged[e.fingerprint].occ = e.occ;
		weights[e.fingerprint] += p2 * e.num_spanning_trees / tcount2;
	}
	double tot_est_occ = 0;
	for (auto& kv : merged) {
		SampleTable::Entry& e = kv.second;
		e.estimate_graph_occurrences = e.sample_count / (s * weights[kv.first]);
		tot_est_occ += e.estimate_graph_occurrences;
	}
	for (auto& kv : merged) {
		SampleTable::Entry e = kv.second;
		e.fingerprint = kv.first;
		e.estimate_graph_frequency = e.estimate_graph_occurrences / tot_est_occ;
		t.addEntry(e);
	}
	return t;
}

/**
 * Prints the table in the natural format.
 */
std::ostream& operator<<(std::ostream& os, const SampleTable& st) {
	for (SampleTable::Entry& e : st.get_entries()) {
		os << e.fingerprint;
		os << "," << e.sample_count;
		os << "," << e.num_spanning_trees;
		os << "," << e.estimate_graph_frequency;
		os << "," << e.estimate_graph_occurrences;
		os << std::endl;
	}
	return os;
}
