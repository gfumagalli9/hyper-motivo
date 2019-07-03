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


void SampleTable::add_entry(SampleTable::Entry e)
{
	entries.push_back(e);
	num_samples += e.sample_count;
}

/**
 * Estimate the total number of occurrences in the graph
 * num_graph_treelets is (an estimate of) the total number of treelets in the graph (not only the colorful ones)
 */
void SampleTable::estimate_occurrences(double num_graph_treelets)
{
	for (auto &e : entries)
		e.estimated_graph_occurrences = (static_cast<double>(e.sample_count) / static_cast<double>(num_samples))
				* (num_graph_treelets / static_cast<double>(e.num_spanning_trees));
}

/**
 * Estimate the relative frequency, from the number of estimated occurrences (i.e. just a normalization)
 */
void SampleTable::estimate_frequencies()
{
	double tot_occ = 0;
	for (Entry &e : entries)
		tot_occ += e.estimated_graph_occurrences;

	if (tot_occ > 0)
		for (Entry &e : entries)
			e.estimated_graph_frequency = e.estimated_graph_occurrences / tot_occ;
}

/**
 * Returns the table's header.
 */
std::string SampleTable::header()
{
	return std::string("footprint, vertices, sample_count, spanning_trees, estimated_frequencies, estimated_occurences");
}

/**
 * Sort entries in nonincreasing order of estimate_graph_occurrences
 */
void SampleTable::sort_by_estimate_occurrences() //FIXME: Use parallel execution policy when implemented in the standard library
{
	std::sort(entries.begin(), entries.end(), [] (const Entry &e1, const Entry &e2) {return e1.estimated_graph_occurrences > e2.estimated_graph_occurrences; });
}

/**
 * Sort entries in nonincreasing order of their binary footprint
 */
void SampleTable::sort_by_footprint() //FIXME: Use parallel execution policy when implemented in the standard library
{
	std::sort(entries.begin(), entries.end(),[] (const Entry &e1, const Entry &e2) { return memcmp(e1.occurrence.binary_footprint(), e2.occurrence.binary_footprint(), Occurrence::binary_footprint_bytes) < 0; });
}


/**
 * Accumulates entries by their footprint. Entries must be sorted in nonincreasing order of their binary footprint
 */
void SampleTable::group_by_footprint()
{
	if (entries.empty())
		return;

	auto it = entries.begin();
	auto result=it;
	while (++it != entries.end())
	{
		if(memcmp(result->occurrence.binary_footprint(), it->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes)==0)
			result->sample_count++;
		else
		    if(++result != it)
		        *result = std::move(*it);
	}

	entries.erase(++result, entries.end());
}

void SampleTable::count_spanning_trees(const TreeletStructureSelector *selector, unsigned int nthreads)
{
	if(entries.empty())
		return;

#ifndef NDEBUG
    const unsigned int size = entries[0].occurrence.get_size();
#endif

	SpanningTreeCounter counter(entries[0].occurrence.get_size(), selector);
	if(nthreads<=1)
	{
		auto it = entries.begin();
		while(it!=entries.end())
		{
		    assert(it->occurrence.get_size()==size);
			uint64_t count = counter.number_of_spanning_trees(it->occurrence);

			it->num_spanning_trees = count;
			while( (++it)!=entries.end() && memcmp(it->occurrence.binary_footprint(), (it-1)->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes)==0)
				it->num_spanning_trees = count;
		}

		return;
	}

	std::vector<std::vector<Entry>::iterator> distinct_footprints;
	auto it = entries.begin();
	distinct_footprints.push_back(it);
	while(++it!=entries.end())
		if(memcmp((it-1)->occurrence.binary_footprint(), it->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes)!=0)
			distinct_footprints.push_back(it);

	distinct_footprints.push_back(it);


	sequencer_t sequencer(0, distinct_footprints.size()-1, nthreads);
	auto threads = new std::thread[nthreads];
	for(unsigned int i=0; i<nthreads; i++)
		threads[i] = std::thread( [this, &distinct_footprints, &sequencer, &counter] { spanning_tree_count_thread(distinct_footprints, sequencer, counter); } );

    for(unsigned int i=0; i<nthreads; i++)
        threads[i].join();

    delete[] threads;
}



void SampleTable::spanning_tree_count_thread(const std::vector<std::vector<Entry>::iterator> &distinct_footprints, sequencer_t &sequencer, SpanningTreeCounter &counter)
{
	while(true)
	{
		sequencer_t::sequence_batch_t batch = sequencer.next_batch();
		if (batch.from >= batch.to_exclusive)
			break;

		for (uint64_t i = batch.from; i<batch.to_exclusive; i++)
		{
			uint64_t count = counter.number_of_spanning_trees(distinct_footprints[i]->occurrence);
			for(auto it=distinct_footprints[i]; it!=distinct_footprints[i+1]; it++)
				it->num_spanning_trees = count;
		}
	}
}

void SampleTable::count_spanning_stars() //FIXME: Make multithreaded?
{
    auto it = entries.begin();
    while(it!=entries.end())
    {
        uint64_t count = SpanningTreeCounter::number_of_spanning_stars(it->occurrence);
        assert(count%it->occurrence.get_size()==0);
        count/=it->occurrence.get_size();

        it->num_spanning_trees = count;
        while( (++it)!=entries.end() && memcmp(it->occurrence.binary_footprint(), (it-1)->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes)==0)
            it->num_spanning_trees = count;
    }
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
		e.estimated_graph_occurrences = static_cast<double>(e.sample_count) / (static_cast<double>(t.num_samples) * weights[kv.first]);
		tot_est_occ += e.estimated_graph_occurrences;
	}

	for (auto& kv : merged)
	{
		SampleTable::Entry e = kv.second;
		e.fingerprint = kv.first;
		e.estimated_graph_frequency = e.estimated_graph_occurrences / tot_est_occ;
		t.add_entry(e);
	}
	return &t;
}


/**
 * Prints the table in the natural format.
 */
std::ostream& operator<<(std::ostream& os, const SampleTable& st)
{
	for (const auto& e : st.entries)
	{
		os << e.occurrence.text_footprint() << ",";

		for(unsigned int i=0; i<e.occurrence.get_size(); i++)
			os << " " << e.occurrence.vertices()[i];

		os << ", " << e.sample_count << ", " << uint128_to_string(e.num_spanning_trees) << ", "
		   << e.estimated_graph_frequency << ", " << e.estimated_graph_occurrences << "\n";
	}
	return os;
}

double SampleTable::norm2() const
{
	double norm2 = 0;
	for (const auto& e : entries)
		norm2 += static_cast<double>(e.sample_count) * static_cast<double>(e.sample_count);

	return std::sqrt(norm2) / static_cast<double>(num_samples);
}

