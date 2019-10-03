// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "SampleTable.h"

#include <map>
#include <cmath>
#include <thread>
#include <cstring>
#include <iomanip>
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


void SampleTable::rescale_occurrences(double factor)
{
    if(double_equality(factor, 1))
        return;

    for (auto &e : entries)
        e.estimated_graph_occurrences *= factor;
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
		        *result = *it; //SampleTable::entry is trivially copyable
	}

	entries.erase(++result, entries.end());
}

void SampleTable::count_rooted_spanning_trees(const TreeletStructureSelector *selector, unsigned int ntherads)
{
	if(entries.empty())
		return;

#ifndef NDEBUG
    const unsigned int size = entries[0].occurrence.get_size();
#endif

	SpanningTreeCounter counter(entries[0].occurrence.get_size(), selector);
	if(ntherads<=1)
	{
		auto it = entries.begin();
		while(it!=entries.end())
		{
		    assert(it->occurrence.get_size()==size);
			uint64_t count = counter.number_of_rooted_spanning_trees(it->occurrence);

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


	sequencer_t sequencer(0, distinct_footprints.size()-1, ntherads);
	auto threads = new std::thread[ntherads];
	for(unsigned int i=0; i<ntherads; i++)
		threads[i] = std::thread( [this, &distinct_footprints, &sequencer, &counter] { spanning_tree_count_thread(distinct_footprints, sequencer, counter); } );

    for(unsigned int i=0; i<ntherads; i++)
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
			uint64_t count = counter.number_of_rooted_spanning_trees(distinct_footprints[i]->occurrence);
			for(auto it=distinct_footprints[i]; it!=distinct_footprints[i+1]; it++)
				it->num_spanning_trees = count;
		}
	}
}

void SampleTable::count_rooted_spanning_stars() //FIXME: Make multithreaded?
{
    auto it = entries.begin();
    while(it!=entries.end())
    {
        uint64_t count = SpanningTreeCounter::number_of_rooted_spanning_stars(it->occurrence);
        assert(count%it->occurrence.get_size()==0);
        count/=it->occurrence.get_size();

        it->num_spanning_trees = count;
        while( (++it)!=entries.end() && memcmp(it->occurrence.binary_footprint(), (it-1)->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes)==0)
            it->num_spanning_trees = count;
    }
}

/// Merges two tables obtained by sampling from different collections of treelts and estimates the number of occurrences of the samples and their frequencies
/// The tables need to be sorted and grouped by footprints and the spanning trees need to be computed w.r.t. the set of treelets in t1 and t2
SampleTable* SampleTable::merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2)
{
    /* Let SH=SH1 + SH2 be the number of samples of H, where SHx is the number of occurrences of H in the x-th table (x=1,2)
     * Let Tx be the number of treelets counted in table x.
     * Let THx be the number of spanning trees of H that are among those considered in table x.
     * Let Sx be the number of samples in table x.
     * Let NH the number of occurrences of H.
     * Let Ejx be the event "the j-th occurrence of H is sampled from x"
     * Let px be the probability that a spanning tree of H considered in table x can actually be sampled
     *
     * E[SH] = E[SH1] + E[SH2]
     *       = \sum_{i=1}^S1 \sum_{j=1}^NH P(Ej1) + \sum_{i=1}^S2 \sum_{j=1}^NH P(Ej2)
     *       = NH * ( (S1 * TH1/T1 * p1) + (S2 * TH2/T2 * p2) )
     *       which implies:
     *       NH = E[SH] / w, where w = (S1 * TH1/T1 * p1) + (S2 * TH2/T2 * p2)
     *       or, equivalently, w =  (S1 * TH1 / tcount1) + (S2 * TH2 / tcount2)
     *       where tcountx = Tx/px is the number of treelets in table x normalized w.r.t. the sampling probability
     */

    SampleTable &t = *(new SampleTable());

    t.num_samples = t1.get_num_samples() + t2.get_num_samples();
    const auto p1 = static_cast<double>(t1.get_num_samples());
    const auto p2 = static_cast<double>(t2.get_num_samples());

    double tot_est_occ = 0;

    auto it1 = t1.begin();
    auto it2 = t2.begin();
    while(it1 != t1.end() || it2 != t2.end())
    {
        Entry e;
        int c;
        if(it1==t1.end())
            c=1;
        else if(it2==t2.end())
            c=-1;
        else
            c=memcmp(it1->occurrence.binary_footprint(), it2->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes);

        double w=0;
        if(c <= 0)
        {
            e.occurrence = it1->occurrence;
            e.sample_count += it1->sample_count;
            w += p1 * static_cast<double>(it1->num_spanning_trees) / tcount1;
            it1++;
        }
        else
            e.occurrence = it2->occurrence;

        if(c >= 0)
        {
            e.sample_count += it2->sample_count;
            w += p2 * static_cast<double>(it2->num_spanning_trees) / tcount2;
            it2++;
        }

        e.estimated_graph_occurrences = static_cast<double>(e.sample_count) / w;
        tot_est_occ += e.estimated_graph_occurrences;
        e.type = 'M';
        t.add_entry(e);
    }

    for (auto &e : t.entries)
        e.estimated_graph_frequency = e.estimated_graph_occurrences / tot_est_occ;

    return &t;
}

/**
 * Weighted average of two count tables.
 * In the output table:
 *   e.sample_count is the sum of the corresponding entries in t1 and t2.
 *	 e.estimated_graph_occurrences = (w1 * t1[e].estimated_graph_occurrences + w2 * t2[e].estimated_graph_occurrences)
 *
 *	 If an occurrence appears in exactly one of t1 and t2, then the only known estimate is used.
 *	 This method also computes the estimated graph frequencies.
 *
 *	 t1 and t2 need to be grouped and sorted by footprints

 */
SampleTable* SampleTable::weighted_average(SampleTable &t1, SampleTable &t2, double w1, double w2)
{
    SampleTable &t = *(new SampleTable());

    double tot_est_occ = 0;

    auto it1 = t1.begin();
    auto it2 = t2.begin();
    while(it1 != t1.end() || it2 != t2.end())
    {
        Entry e;
        int c;
        if(it1==t1.end())
            c=1;
        else if(it2==t2.end())
            c=-1;
        else
            c=memcmp(it1->occurrence.binary_footprint(), it2->occurrence.binary_footprint(), Occurrence::binary_footprint_bytes);


        if(c < 0)
        {
            e.occurrence = it1->occurrence;
            e.sample_count = it1->sample_count;
            e.estimated_graph_occurrences = it1->estimated_graph_occurrences;
            it1++;
        }
        else if(c>0)
        {
            e.occurrence = it2->occurrence;
            e.sample_count = it2->sample_count;
            e.estimated_graph_occurrences += it2->estimated_graph_occurrences;
            it2++;
        }
        else
        {
            e.occurrence = it1->occurrence;
            e.sample_count = it1->sample_count + it2->sample_count;
            e.estimated_graph_occurrences = w1*it1->estimated_graph_occurrences + w2*it2->estimated_graph_occurrences;
            it1++;
            it2++;
        }

        e.type = 'W';
        tot_est_occ += e.estimated_graph_occurrences;
        t.add_entry(e);
    }

    for (auto &e : t.entries)
        e.estimated_graph_frequency = e.estimated_graph_occurrences / tot_est_occ;

    return &t;
}


/**
 * Prints the table in the natural format.
 */
std::ostream& operator<<(std::ostream& os, const SampleTable& st)
{
    std::ios_base::fmtflags flags( os.flags() );
    os << std::scientific << std::setprecision(4) << std::setfill('0');

	for(const auto& e : st.entries)
	{
        os << e.occurrence.text_footprint()
           << ", " << e.estimated_graph_occurrences
           << ", " << e.estimated_graph_frequency
           << ", " << e.sample_count
           << ", " << e.type
           << ", " << uint128_to_string(e.num_spanning_trees) << ",";

        for(unsigned int i=0; i<e.occurrence.get_size(); i++)
		    os << " " << e.occurrence.vertices()[i];

        os << "\n";
	}

    os.flags( flags ); 
	return os;
}

double SampleTable::norm2() const
{
	double norm2 = 0;
	for (const auto& e : entries)
		norm2 += static_cast<double>(e.sample_count) * static_cast<double>(e.sample_count);

	return std::sqrt(norm2) / static_cast<double>(num_samples);
}

