/*
* CachedSTC.cpp
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#include "CachedSTC.h"
#include "ColorCodingSpanningTreeCounter.h"

struct vertex_info {
	char* ptr;
	uint64_t count = 0;
};

/**
 * Compute the spanning tree table of a graphlet
 */
CachedSTC::treelet_table_t* CachedSTC::compute_t_table(const Occurrence &o) {
	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
	CachedSTC::treelet_table_t* tab = new treelet_table_t();
	tab->set_empty_key(Treelet::invalid_treelet);
	ColorCodingSpanningTreeCounter ccstc(&o, nullptr);
	ccstc.count();
	ccstc.get_table(tab);
	std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
	tot_running_time += delta_t.count();
	return tab;
}

/**
 * Update the reverse table (treelet to graphlet) using a treelet count table and a given graphlet
 */
void CachedSTC::update_reverse_table(treelet_table_t& tab, const Occurrence& o) {
	for (const auto &it : tab) { // update the reverse table
		if (reverse_table.count(it.first) == 0) {
			reverse_table[it.first] = new occ_table_t();
			reverse_table[it.first]->set_empty_key(Occurrence());
		}
		(*reverse_table[it.first])[o] = it.second;
	}
}

