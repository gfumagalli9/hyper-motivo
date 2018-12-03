/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include <string>
#include "SpanningTreeCounter.h"
#include "graph/FullGraphColoring.h"
#include "treelets/TreeletTable.h"
#include "treelets/TreeletTableCollection.h"
#include "../builder/SimpleTreeletTableBuilder.h"
#include "../builder/TreeletTableBuilder.h"
#include "../common/graph/SimpleGraph.h"
#include "../common/common.h"
#include "../sampler/ColorCodingSpanningTreeCounter.h"
#include "../common/CachedSTC.h"

struct vertex_info {
	char* ptr;
	uint64_t count = 0;
};

/**
 * Compute the number of spanning trees of the occurrence
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ) {
	return occ.number_of_spanning_trees();
}

/**
 * Compute the number of spanning trees of the occurrence, excluding stars
 */
uint64_t SpanningTreeCounter::num_spanning_trees_nostars(const Occurrence& occ) {
	return occ.number_of_spanning_trees() - num_spanning_stars(occ);
}

/**
 * Compute the number of spanning trees of the occurrence, possibly including/excluding some
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ, const TreeletSelector* ts) {
	if (ts == nullptr || ts->get_size() == 0)
		return occ.number_of_spanning_trees();
	CachedSTC::treelet_table_t* tab = new CachedSTC::treelet_table_t();
	tab->set_empty_key(Treelet::invalid_treelet);
	ColorCodingSpanningTreeCounter ccstc(&occ, ts);
	ccstc.count();
	return ccstc.number_of_spanning_trees();
}

/**
 * Return the number of spanning stars
 */
unsigned int SpanningTreeCounter::num_spanning_stars(const Occurrence& occ)
{
	UndirectedGraph h(occ);
	unsigned int count = 0;
	for (UndirectedGraph::vertex_t v = 0; v < h.number_of_vertices(); v++)
		count += (h.degree(v) == h.number_of_vertices() - 1) ? 1u : 0u;

	return count;
}
