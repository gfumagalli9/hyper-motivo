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

#ifndef SRC_SAMPLER_SAMPLETABLE_H_
#define SRC_SAMPLER_SAMPLETABLE_H_

#include <ostream>
#include <string>
#include <vector>
#include <dense_hash_map>
#include "../common/treelets/TreeletStructureSelector.h"
#include "Occurrence.h"
#include "DynamicSequencer.h"
#include "SpanningTreeCounter.h"

class SampleTable
{
public:
    class Entry // a table entry
    {
    public:
        Occurrence occurrence;
        uint64_t num_spanning_trees = 0;
        uint64_t sample_count = 0;
        double estimated_graph_frequency = 0;
        double estimated_graph_occurrences = 0;
        char type = '?';
    };

    typedef std::vector<Entry>::const_iterator const_iterator;

    static constexpr const char* header = "motif, est_occurrences, est_frequency, samples, sampling_algo, spanning_trees, vertices";

private:
    std::vector<Entry> entries;
    uint64_t num_samples = 0;
    typedef DynamicSequencer<uint64_t> sequencer_t;

    void spanning_tree_count_thread(const std::vector<std::vector<Entry>::iterator> &distinct_footprints, sequencer_t &sequencer, SpanningTreeCounter &counter);


public:
	SampleTable() = default;

    void add_entry(Entry e);

    template<typename Iterator> void add_occurrences(const Iterator first, const Iterator end, const char type)
    {
        for(Iterator it=first; it!=end; it++)
        {
            SampleTable::Entry e;
            e.occurrence = *it;
            e.sample_count = 1;
            e.type = type;
            entries.push_back(e);
            num_samples++;
        }
    }

    void count_rooted_spanning_trees(const TreeletStructureSelector *selector, unsigned int ntherads);

    void count_rooted_spanning_stars();

    void estimate_occurrences(double num_graph_treelets);

	void estimate_frequencies();

    void rescale_occurrences(double factor);

    void sort_by_estimate_occurrences();

    void sort_by_footprint();

    void group_by_footprint();

    static SampleTable* merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2); // merge two tables (see source for details)

    static SampleTable* weighted_average(SampleTable &t1, SampleTable &t2, double w1, double w2); // average two tables (see source for details)

    friend std::ostream& operator<<(std::ostream& os, const SampleTable& st);

	uint64_t get_num_samples() const
    {
		return num_samples;
	}

	uint64_t size() const
    {
		return entries.size();
	}

	double norm2() const;

	const_iterator begin() const { return entries.cbegin(); };
    const_iterator end() const { return entries.cend(); }
};

#endif /* SRC_SAMPLER_SAMPLETABLE_H_ */
