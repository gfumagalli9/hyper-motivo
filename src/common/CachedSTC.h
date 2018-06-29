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

#include <string>
#include <map>
#include "graph/FullGraphColoring.h"
#include "treelets/TreeletTable.h"
#include "treelets/TreeletTableCollection.h"
#include "../builder/SimpleTreeletTableBuilder.h"
#include "../builder/TreeletTableBuilder.h"
#include "../common/common.h"
#include "../sampler/OccurrenceSampler.h"

class CachedSTC {
//	typedef google::dense_hash_map<Treelet, uint64_t> table_t;
//	static OccurrenceSampler::OccurrenceHash hasher = OccurrenceSampler::OccurrenceHash(true, false);
	std::map<Occurrence, std::map<Treelet, uint64_t>*, Occurrence::OccurrenceCompare> table;
	std::map<Treelet, uint64_t>* compute_t_table(const Occurrence &o);
public:
	CachedSTC();
	~CachedSTC();
	uint64_t num_spanning_trees(const Occurrence &o, const Treelet &t);
	inline std::map<Treelet, uint64_t>* get_t_table(const Occurrence &o) {
		return table.count(o) ? table[o] : nullptr;
	}
};

#endif /* SRC_COMMON_CACHEDSTC_H_ */
