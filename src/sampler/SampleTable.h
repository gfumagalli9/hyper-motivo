/*
 * SampleTable.h
 *
 *  Created on: 30 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SAMPLETABLE_H_
#define SRC_SAMPLER_SAMPLETABLE_H_

#include "Occurrence.h"
#include <ostream>
#include <string>
#include <vector>
#include <sparsehash/dense_hash_map>

class SampleTable
{
public:
    class Entry // a table entry
    {
    public:
        Occurrence occurrence;
        std::string fingerprint = "";
        uint128_t num_spanning_trees = 0;
        uint64_t sample_count = 0;
        double estimate_graph_frequency = 0;
        double estimate_graph_occurrences = 0;
    };

    typedef std::vector<Entry>::const_iterator const_iterator;


private:
    std::vector<Entry> entries;
    uint64_t num_samples = 0;


public:
	SampleTable() = default;

    void addEntry(Entry e);

    void estimateOccurrences(double num_graph_treelets, unsigned int k, bool store_only_0 = false);
	void estimateFrequencies();

	void sort_by_estimate_occ();

    std::string header();

    static SampleTable merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2); // merge two tables (see source for details)
    static SampleTable average(SampleTable& t1, SampleTable& t2, double w1, double w2); // average two tables (see source for details)

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
