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
#include "OccurrenceSampler.h"
#include "Occurrence.h"

class SampleTable {
public:
	typedef OccurrenceSampler::table_t table_t;
	class Entry { // a table entry
	public:
		Occurrence occ;
		std::string fingerprint = "";
		uint128_t num_spanning_trees = 0;
		uint64_t sample_count = 0;
		double estimate_graph_frequency = 0;
		double estimate_graph_occurrences = 0;
	};
	SampleTable();
	~SampleTable();
	SampleTable(table_t *t, TreeletSelector *ts = nullptr); // build from an (Occurrence,count) table;
	void addEntry(Entry e);
	void estimateOccurrences(double num_graph_treelets, bool store_only_0 = false);
	std::string header();
	void sort_by_fingerprint();
	void sort_by_sample_count();
	void sort_by_estimate_occ();
	void sort_by_estimate_freq();
	static SampleTable merge(SampleTable& t1, SampleTable& t2, double tcount1, double tcount2); // merge two tables (see source for details)
	friend std::ostream& operator<<(std::ostream& os, const SampleTable& st);
private:
	std::vector<Entry> entries;
	uint64_t num_samples = 0;
public:
	std::vector<Entry> get_entries() const {
		return entries;
	}
	uint64_t get_num_samples() const {
		return num_samples;
	}
	uint32_t size() const {
		return entries.size();
	}
};

#endif /* SRC_SAMPLER_SAMPLETABLE_H_ */
