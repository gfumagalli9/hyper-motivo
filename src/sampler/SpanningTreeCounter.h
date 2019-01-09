/*
 * SpanningTreeCounter.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SPANNINGTREECOUNTER_H_
#define SRC_SAMPLER_SPANNINGTREECOUNTER_H_

#include "Occurrence.h"
#include "../common/treelets/TreeletSelector.h"
#include <google/dense_hash_map>
#include <mutex>
#include <cstdio>

/**
 * Counts the spanning trees of an occurrence via color-coding
 */
class SpanningTreeCounter {
private:
	typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_table_t;
	occ_table_t cache;
	std::mutex m_mutex;

public:
	SpanningTreeCounter()
	{
		cache.set_empty_key(Occurrence());
	}

	/**
	 * Static methods: on-the-fly computation
	 */
	static uint64_t num_spanning_trees(const Occurrence &occ);
	static uint64_t num_spanning_trees(const Occurrence &occ, const TreeletSelector *ts);
	static unsigned int num_spanning_stars(const Occurrence &occ);
	static uint64_t num_spanning_trees_nostars(const Occurrence &occ);

	/**
	 * Instance methods: use the cache
	 */
	uint64_t get_spanning_trees(const Occurrence &occ, const TreeletSelector *ts);

	uint64_t size() {
		return cache.size();
	}

	/**
	 * Save to file, in binary format.
	 * The record format is:
	 * <k, fingerprint, edges>
	 */
	void save_to_file(std::string filename);

	/**
	 * Read from file
	 * The record format is:
	 * <k, fingerprint, edges>
	 */
	void read_from_file(std::string filename);
};

#endif /* SRC_SAMPLER_SPANNINGTREECOUNTER_H_ */
