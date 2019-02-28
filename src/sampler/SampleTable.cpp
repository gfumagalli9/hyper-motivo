/*
 * SampleTable.cpp
 *
 *  Created on: 30 mag 2018
 *      Author: brix
 */
#include "SampleTable.h"

#include <map>
#include <cmath>
#include <thread>
#include "../common/util.h"


void SampleTable::addEntry(SampleTable::Entry e)
{
	entries.push_back(e);
	num_samples += e.sample_count;
}

/**
 * Estimate the total number of occurrences in the graph
 * num_graph_treelets is (an estimate of) the total number of treelets in the graph (not only the colorful ones)
 */
void SampleTable::estimateOccurrences(double num_graph_treelets)
{
	for (auto &e : entries)
		e.estimate_graph_occurrences = (static_cast<double>(e.sample_count) / static_cast<double>(num_samples))
				* (num_graph_treelets / static_cast<double>(e.num_spanning_trees));
}

/**
 * Estimate the relative frequency, from the number of estimated occurrences (i.e. just a normalization)
 */
void SampleTable::estimateFrequencies()
{
	double tot_occ = 0;
	for (Entry &e : entries)
		tot_occ += e.estimate_graph_occurrences;

	if (tot_occ > 0)
		for (Entry &e : entries)
			e.estimate_graph_frequency = e.estimate_graph_occurrences / tot_occ;
}

/**
 * Returns the table's header.
 */
std::string SampleTable::header()
{
	return std::string("motif,sample_count,spanning_trees,estim_freq,estim_occur");
}

/**
 * Sort entries in nonincreasing order of estimate_graph_occurrences
 */
void SampleTable::sort_by_estimate_occ()
{
	std::sort(entries.begin(), entries.end(), [] (const Entry &e1, const Entry &e2) {return e1. estimate_graph_occurrences > e2.estimate_graph_occurrences;});
}

/**
 * Merge two tables.
 * tcount1 and tcount2 are the total treelet counts (the number of colorful k-treelets
 *	the sampling was performed on, for t1 and t2 respectively).
 * Graphlet occurrences can have a different number of spanning trees in the two tables.
 * In the output table, e.sample_count is the sum of the corresponding entries in t1 and t2.
 * If this is the case, then in the merged table:
 *	e.estimate_graph_frequency is obtained as an appropriate average of the two tables
 *	e.estimate_graph_occurrences is obtained as an appropriate average of the two tables
 *
 */
SampleTable* SampleTable::merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2)
{
	SampleTable &t = *(new SampleTable());
	std::map<std::string, SampleTable::Entry> merged;
	std::map<std::string, double> weights;
	t.num_samples = t1.get_num_samples() + t2.get_num_samples();
	double p1 = static_cast<double>(t1.get_num_samples()) / static_cast<double>(t.num_samples);
	double p2 = static_cast<double>(t2.get_num_samples()) / static_cast<double>(t.num_samples);

	for (const SampleTable::Entry &e : t1.entries)
	{
		merged[e.fingerprint].sample_count += e.sample_count;
		weights[e.fingerprint] += p1 * static_cast<double>(e.num_spanning_trees) / tcount1;
	}

	for (const SampleTable::Entry &e : t2.entries)
	{
		merged[e.fingerprint].sample_count += e.sample_count;
		weights[e.fingerprint] += p2 * static_cast<double>(e.num_spanning_trees) / tcount2;
	}

	double tot_est_occ = 0;
	for (auto& kv : merged)
	{
		SampleTable::Entry& e = kv.second;
		e.estimate_graph_occurrences = static_cast<double>(e.sample_count) / (static_cast<double>(t.num_samples) * weights[kv.first]);
		tot_est_occ += e.estimate_graph_occurrences;
	}

	for (auto& kv : merged)
	{
		SampleTable::Entry e = kv.second;
		e.fingerprint = kv.first;
		e.estimate_graph_frequency = e.estimate_graph_occurrences / tot_est_occ;
		t.addEntry(e);
	}
	return &t;
}

/**
 * Weighted average of two count tables.
 * In the output table:
 *   e.sample_count is the sum of the corresponding entries in t1 and t2.
 *	 e.estimate_graph_frequency = (w1 * t1[e].estimate_graph_frequency + w2 * t2[e].estimate_graph_frequency)
 *	 e.estimate_graph_occurrences = [the same as above]
 *   e.num_spanning_trees = -1
 */ //FIXME: Why is the fingerprint used as a key?
SampleTable SampleTable::average(SampleTable& t1, SampleTable& t2, double w1, double w2)
{
	SampleTable t;
	std::map<std::string, SampleTable::Entry> merged;

	for (SampleTable::Entry e : t1.entries) {
		merged[e.fingerprint].fingerprint = e.fingerprint;
		merged[e.fingerprint].num_spanning_trees = 1;
		merged[e.fingerprint].sample_count += e.sample_count;
		merged[e.fingerprint].estimate_graph_frequency = e.estimate_graph_frequency;
		merged[e.fingerprint].estimate_graph_occurrences = e.estimate_graph_occurrences;
	}
	for (SampleTable::Entry e : t2.entries) {
		merged[e.fingerprint].fingerprint = e.fingerprint;
		merged[e.fingerprint].sample_count += e.sample_count;
		if (merged[e.fingerprint].num_spanning_trees != 1) { // not in t1
			merged[e.fingerprint].estimate_graph_frequency = e.estimate_graph_frequency;
			merged[e.fingerprint].estimate_graph_occurrences = e.estimate_graph_occurrences;
		} else {
			merged[e.fingerprint].estimate_graph_frequency *= w1;
			merged[e.fingerprint].estimate_graph_occurrences *= w1;
			merged[e.fingerprint].estimate_graph_frequency += w2 * e.estimate_graph_frequency;
			merged[e.fingerprint].estimate_graph_occurrences += w2 * e.estimate_graph_occurrences;
		}
	}
	for (auto &it : merged) {
		merged[it.first].num_spanning_trees = 0;
		t.addEntry(merged[it.first]);
	}
	t.estimateFrequencies();
	return t;
}

/**
 * Prints the table in the natural format.
 */
std::ostream& operator<<(std::ostream& os, const SampleTable& st)
{
	for (const auto& e : st.entries)
		os << e.fingerprint << "," << e.sample_count << "," << uint128_to_string(e.num_spanning_trees) << "," << e.estimate_graph_frequency << "," << e.estimate_graph_occurrences << "\n";

	return os;
}

double SampleTable::norm2() const
{
	double norm2 = 0;
	for (const auto& e : entries)
		norm2 += static_cast<double>(e.sample_count) * static_cast<double>(e.sample_count);

	return std::sqrt(norm2) / static_cast<double>(num_samples);
}

void SampleTable::count_spanning_trees(const unsigned int size, const TreeletStructureSelector *selector, unsigned int nthreads)
{
    SpanningTreeCounter counter(size, selector);
    if(nthreads<=1)
    {
        for(Entry &e : entries)
            e.num_spanning_trees = counter.number_of_spanning_trees(e.occurrence);

        return;
    }

    sequencer_t sequencer(0, entries.size(), nthreads);
    auto threads = new std::thread[nthreads];
    for(unsigned int i=0; i<nthreads; i++)
        threads[i] = std::thread( [this, &sequencer, &counter] { spanning_tree_count_thread(sequencer, counter); } );
}



void SampleTable::spanning_tree_count_thread(sequencer_t &sequencer, SpanningTreeCounter &counter)
{
    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer.next_batch();
        if (batch.from >= batch.to_exclusive)
            break;

        for (uint64_t i = batch.from; i<batch.to_exclusive; i++)
            entries[i].num_spanning_trees = counter.number_of_spanning_trees(entries[i].occurrence);
    }
}
