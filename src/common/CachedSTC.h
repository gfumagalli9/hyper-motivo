/*
 * CachedSTC.h
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 *
 *  This is a "cached" spanning tree counter. It can tell how many treelets of type T span a graphlet of
 *  type C. It does so by computing all treelet counts for C the first time a query (C,T) is made, and then
 *  stores the results for later re-querying.
 */

#ifndef SRC_COMMON_CACHEDSTC_H_
#define SRC_COMMON_CACHEDSTC_H_

#include <google/dense_hash_map>
#include <string>
#include <map>
#include <mutex>
#include "graph/FullGraphColoring.h"
#include "treelets/TreeletTable.h"
#include "treelets/TreeletTableCollection.h"
#include "../builder/SimpleTreeletTableBuilder.h"
#include "../builder/TreeletTableBuilder.h"
#include "../common/common.h"
#include "../sampler/OccurrenceSampler.h"

class CachedSTC
{
public:

	struct OcurrenceFootprintLess
	{
		inline bool operator()[[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
		{
			return (occ1.is_valid()==occ2.is_valid()) && (memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes) < 0);
		}
	};

//	typedef google::dense_hash_map<Treelet, uint64_t, Treelet::TreeletHash, Treelet::compare_eq> treelet_table_t;
//	typedef google::dense_hash_map<Occurrence, treelet_table_t*, Occurrence::OccurrenceHash,
//			Occurrence::compare_eq> occurrence_treelet_table_t;
	typedef std::map<Treelet, uint64_t, Treelet::compare_less> treelet_table_t;
	typedef std::map<Occurrence, treelet_table_t*, OcurrenceFootprintLess> occurrence_treelet_table_t;

private:
	occurrence_treelet_table_t table;
	treelet_table_t* compute_t_table(const Occurrence &o);
	std::mutex m_mutex;
	double tot_running_time = 0;
	double tot_computing_time = 0;
public:
	uint64_t num_spanning_trees(const Occurrence &o, const Treelet &t);

	double running_time() {
		return tot_running_time;
	}

	double computing_time() {
		return tot_computing_time;
	}

	/**
	 * Return the spanning tree table of the graphlet.
	 */
	treelet_table_t* const get_t_table(const Occurrence &o) {
		if (!table.count(o)) {
			m_mutex.lock();
			table[o] = compute_t_table(o);
			m_mutex.unlock();
		}
		return table[o];
	}

};

#endif /* SRC_COMMON_CACHEDSTC_H_ */
