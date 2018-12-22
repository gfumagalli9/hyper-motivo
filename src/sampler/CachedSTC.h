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

#include "../../../../../../usr/include/google/dense_hash_map"
#include "../../../../../../usr/include/c++/8/string"
#include "../../../../../../usr/include/c++/8/map"
#include "../../../../../../usr/include/c++/8/mutex"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/common.h"
#include "OccurrenceSampler.h"

class CachedSTC
{
public:
	typedef google::dense_hash_map<Treelet, uint64_t, Treelet::TreeletHash, Treelet::compare_eq> treelet_table_t;
	typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_table_t;
    typedef google::dense_hash_map<Treelet, occ_table_t*, Treelet::TreeletHash, Treelet::compare_eq> treelet_occurrence_table_t;
    typedef google::dense_hash_map<Occurrence, treelet_table_t*, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occurrence_treelet_table_t;
//	typedef std::map<Treelet, uint64_t, Treelet::compare_less> treelet_table_t;
//	typedef std::map<Occurrence, treelet_table_t*, OcurrenceFootprintLess> occurrence_treelet_table_t;

private:
	occurrence_treelet_table_t table;
	treelet_occurrence_table_t reverse_table;
	treelet_table_t* compute_t_table(const Occurrence &o);
	std::mutex m_mutex;
	double tot_running_time = 0;
	double tot_computing_time = 0;

public:
	CachedSTC() {
		table.set_empty_key(Occurrence());
		reverse_table.set_empty_key(Treelet::invalid_treelet);
	}

	~CachedSTC() {
		for (auto &itr : table)
			delete itr.second;
		for (auto &itr : reverse_table)
			delete itr.second;
	}

	double running_time() {
		return tot_running_time;
	}

	double computing_time() {
		return tot_computing_time;
	}

	/**
	 * Return the spanning tree table of the graphlet.
	 */
	inline treelet_table_t* get_t_table(const Occurrence &o) {
		if (!table.count(o)) {
			auto tb = compute_t_table(o);
			m_mutex.lock();
			table[o] = tb;
			update_reverse_table(*tb, o);
			m_mutex.unlock();
		}
		return table[o];
	}

	/**
	 * Update all tables to include a given graphlet.
	 */
	inline void update_tables(const Occurrence &o) {
		if (!table.count(o)) {
			auto tb = compute_t_table(o);
			m_mutex.lock();
			table[o] = tb;
			update_reverse_table(*tb, o);
			m_mutex.unlock();
		}
	}

	void update_reverse_table(treelet_table_t& tab, const Occurrence& o);

	/**
	 * Return the graphlets present in the table and spanned by the given tree, with spanning counts.
	 */
	inline occ_table_t* get_reverse_table(const Treelet &t) {
		return reverse_table[t];
	}

//	uint64_t num_spanning_trees(const Occurrence &o, const Treelet &t);

};

#endif /* SRC_COMMON_CACHEDSTC_H_ */
