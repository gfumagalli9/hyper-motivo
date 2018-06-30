/*
 * SimpleTreeletTableBuilder.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_BUILDER_SIMPLETREELETTABLEBUILDER_H_
#define SRC_BUILDER_SIMPLETREELETTABLEBUILDER_H_

#include <sparsehash/sparse_hash_map>
#include <sparsehash/dense_hash_map>
#include "../common/treelets/Treelet.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/GraphColoring.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletSelector.h"

class SimpleTreeletTableBuilder {

private:
	const UndirectedGraph* graph;
	const GraphColoring* coloring;
	const unsigned int size;
	const TreeletTableCollection* lower;
    std::ostream* output;
	const bool store_0_only;
	TreeletSelector* selector;

public:

#ifdef MOTIVO_DENSE_HASHMAP
	typedef google::dense_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#define BUILDER_INIT_HASHMAP(hm) do { (hm).set_empty_key(Treelet::invalid_treelet); } while(false)
#else
	typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#define BUILDER_INIT_HASHMAP(hm) do {} while(false)
#endif

	SimpleTreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
			const TreeletTableCollection* lower, std::ostream* output, const bool store_0_only=false, TreeletSelector* selector=nullptr);
	~SimpleTreeletTableBuilder();
	void build();
	/// Fills a size-1 table
	void do_build_1_st[[gnu::hot,gnu::flatten]]();
	/// Fills a table for sizes > 1
	void do_build_st [[gnu::hot,gnu::flatten]]();
    inline std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const UndirectedGraph::vertex_t u, const table_t &table);
	inline void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);
};

#endif /* SRC_BUILDER_SIMPLETREELETTABLEBUILDER_H_ */
