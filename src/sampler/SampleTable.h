/*
 * SampleTable.h
 *
 *  Created on: 30 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SAMPLETABLE_H_
#define SRC_SAMPLER_SAMPLETABLE_H_

#include <ostream>
#include <string>
#include <vector>
#include <sparsehash/dense_hash_map>
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
        Occurrence occurrence; //FIXME: Remove?
        std::string fingerprint = "";
        uint64_t num_spanning_trees = 0;
        uint64_t sample_count = 0;
        double estimated_graph_frequency = 0;
        double estimated_graph_occurrences = 0;
    };

    typedef std::vector<Entry>::const_iterator const_iterator;

private:
    std::vector<Entry> entries;
    uint64_t num_samples = 0;
    typedef DynamicSequencer<uint64_t> sequencer_t;

    void spanning_tree_count_thread(const std::vector<std::vector<Entry>::iterator> &distinct_footprints, sequencer_t &sequencer, SpanningTreeCounter &counter);


public:
	SampleTable() = default;

    void add_entry(Entry e);

    template<typename Iterator> void add_occurrences(const Iterator first, const Iterator end)
    {
        for(Iterator it=first; it!=end; it++)
        {
            SampleTable::Entry e;
            e.occurrence = *it;
            e.fingerprint = it->text_footprint();
            e.sample_count = 1;
            entries.push_back(e);
            num_samples++;
        }
    }

    void count_spanning_trees(const TreeletStructureSelector *selector, unsigned int ntherads);

    void count_spanning_stars();

    void estimate_occurrences(double num_graph_treelets);

	void estimate_frequencies();

    void sort_by_estimate_occurrences();

    void sort_by_footprint();

    void group_by_footprint();

    std::string header();

    static SampleTable* merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2); // merge two tables (see source for details)

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
