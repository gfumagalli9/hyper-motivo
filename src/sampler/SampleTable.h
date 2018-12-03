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
#include <sparsehash/dense_hash_map>
#include "OccurrenceSampler.h"
#include "Occurrence.h"

class SampleTable
{
public:
    //typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceHash, Occurrence::compare_eq> table_t;

    class Entry // a table entry
    {
    public:
        Occurrence occ;
        std::string fingerprint = "";
        uint128_t num_spanning_trees = 0;
        uint64_t sample_count = 0;
        double estimate_graph_frequency = 0;
        double estimate_graph_occurrences = 0;
    };

private:
    std::vector<Entry> entries;
    uint64_t num_samples = 0;


public:
	SampleTable() = default;
	SampleTable(Occurrence *occurrences, uint64_t noccurrences, TreeletSelector *ts = nullptr); // build from list of Occurrecens;

    void addEntry(Entry e);

    void estimateOccurrences(double num_graph_treelets, unsigned int k, bool store_only_0 = false);
	void estimateFrequencies();
	void update_spanning_trees(TreeletSelector *ts);

	void sort_by_estimate_occ();

    std::string header();

    static SampleTable merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2); // merge two tables (see source for details)
    static SampleTable average(SampleTable& t1, SampleTable& t2, double w1, double w2); // average two tables (see source for details)
	static SampleTable saverage(SampleTable& t1, SampleTable& t2);
	friend std::ostream& operator<<(std::ostream& os, const SampleTable& st);

	uint64_t get_num_samples() const
    {
		return num_samples;
	}

	uint64_t size() const
    {
		return entries.size();
	}

	const Entry* get_entries() const
    {
        return entries.data();
    }
};

#endif /* SRC_SAMPLER_SAMPLETABLE_H_ */
