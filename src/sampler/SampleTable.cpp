/*
 * SampleTable.cpp
 *
 *  Created on: 30 mag 2018
 *      Author: brix
 */

#include <map>
#include "SampleTable.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"


/**
 * Take an Occurrence collection and calculates spanning trees, estimates frequencies, etc
 */
SampleTable::SampleTable(Occurrence *occurrences, uint64_t noccurrences, TreeletSelector *ts)
{
    num_samples = noccurrences;
    if (noccurrences == 0)
		return;

	unsigned int k = occurrences->get_size(); // graphlet size

    //	// Let's check if the TreeletSelector is including/excluding just k-stars...
    bool include_only_stars = false;
    bool exclude_only_stars = false;
    if(ts!=nullptr && ts->get_treelet_size() == k && ts->get_size() == 2 && ts->get_treelets()[0].is_star() && ts->get_treelets()[1].is_star())
    {
        if(ts->get_mode() == TreeletSelector::MODE_INCLUDE)
            include_only_stars = true;
        else
            exclude_only_stars = true;
    }


    //Aggregate occurrences by footprint /
    google::dense_hash_map<Occurrence*, uint64_t, OccurrenceFootprintHash, OccurrenceFootprintEquality> ht(noccurrences);
    Occurrence empty; //FIXME?
    ht.set_empty_key(&empty);
    for(uint64_t i=0; i<noccurrences; i++)
        ht[&occurrences[i]]+=1;

    SpanningTreeCounter stc;
    double normalized_samples = 0;
	// populate the table
	for(const auto& kv : ht)
	{
		Entry e;
		//e.occ = occurrences[i];
		e.fingerprint =  kv.first->text_footprint();
		e.sample_count = kv.second;

		if (exclude_only_stars)
			e.num_spanning_trees = stc.num_spanning_trees_nostars(*kv.first);
		else if (include_only_stars)
			e.num_spanning_trees = stc.num_spanning_stars(*kv.first);
		else
			e.num_spanning_trees = stc.num_spanning_trees(*kv.first, ts);

		normalized_samples += static_cast<double>(e.sample_count) / static_cast<double>(e.num_spanning_trees);
		entries.push_back(e);
	}

	// compute the estimate graph frequency
	for (Entry &e : entries)
		e.estimate_graph_frequency = static_cast<double>(e.sample_count) / (static_cast<double>(e.num_spanning_trees) * normalized_samples);
}

void SampleTable::addEntry(SampleTable::Entry e) {
	entries.push_back(e);
	num_samples += e.sample_count;
}

/**
 * Estimate the total number of occurrences in the graph
 * num_graph_treelets is (an estimate of) the total number of treelets in the graph (not only the colorful ones)
 */
void SampleTable::estimateOccurrences(double num_graph_treelets, unsigned int k, bool store_only_0)
{
	for (auto &e : entries)
		e.estimate_graph_occurrences = (static_cast<double>(e.sample_count) / static_cast<double>(num_samples)) *
		        (num_graph_treelets / static_cast<double>(e.num_spanning_trees * (store_only_0 ? 1 : k)));
}

/**
 * Estimate the relative frequency, from the number of estimated occurrences (i.e. just a normalization)
 */
void SampleTable::estimateFrequencies()
{
	double tot_occ = 0;

	for (Entry &e : entries)
		tot_occ += e.estimate_graph_occurrences;

	if (tot_occ>0)
		for (Entry &e : entries)
			e.estimate_graph_frequency = e.estimate_graph_occurrences / tot_occ;
}

/**
 * Returns the table's header.
 */
std::string SampleTable::header() {
	return std::string("motif,sample_count,spanning_trees,estim_freq,estim_occur");
}

/**
 * Sort entries in nonincreasing order of estimate_graph_occurrences
 */
void SampleTable::sort_by_estimate_occ()
{
    std::sort(entries.begin(), entries.end(),
              [] (const Entry &e1, const Entry &e2) { return e1. estimate_graph_occurrences > e2.estimate_graph_occurrences; }  );
}

/**
 * Merge two tables.
 * tcount1 and tcount2 are the total treelet counts (the number of colorful k-treelets
 *	the sampling was performed on, for t1 and t2 respectively).
 * In the output table, e.sample_count is the sum of the corresponding entries in t1 and t2.
 * If in the two tables the entries have a different number of spanning trees.
 * If this is the case, then in the merged table:
 * 	e.num_spanning_trees = -1
 *	e.estimate_graph_frequency is obtained as an appropriate average of the two tables
 *	e.estimate_graph_occurrences is obtained as an appropriate average of the two tables
 *
 */
SampleTable SampleTable::merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2)
{
	SampleTable t;
	std::map<std::string, SampleTable::Entry> merged;
	std::map<std::string, double> weights;
	uint64_t s = t.num_samples = t1.get_num_samples() + t2.get_num_samples();
	double p1 = static_cast<double>(t1.get_num_samples()) / static_cast<double>(s);
	double p2 = static_cast<double>(t2.get_num_samples()) / static_cast<double>(s);

	for (SampleTable::Entry e : t1.entries)
	{
		merged[e.fingerprint].sample_count += e.sample_count;
		//merged[e.fingerprint].occ = e.occ;
		weights[e.fingerprint] += p1 * static_cast<double>(e.num_spanning_trees) / tcount1;
	}

	for (SampleTable::Entry e : t2.entries)
	{
		merged[e.fingerprint].sample_count += e.sample_count;
		//merged[e.fingerprint].occ = e.occ;
		weights[e.fingerprint] += p2 * static_cast<double>(e.num_spanning_trees) / tcount2;
	}

	double tot_est_occ = 0;
	for (auto& kv : merged)
	{
		SampleTable::Entry& e = kv.second;
		e.estimate_graph_occurrences = static_cast<double>(e.sample_count) / (static_cast<double>(s) * weights[kv.first]);
		tot_est_occ += e.estimate_graph_occurrences;
	}

	for (auto& kv : merged)
	{
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
std::ostream& operator<<(std::ostream& os, const SampleTable& st)
{
	for (const SampleTable::Entry& e : st.entries) {
		os << e.fingerprint
		   << "," << e.sample_count
           << "," << e.num_spanning_trees
           << "," << e.estimate_graph_frequency
           << "," << e.estimate_graph_occurrences
           << "\n";
	}
	return os;
}
