//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELETTABLE_H
#define MOTIVO_TREELETTABLE_H

#include <sparsehash/sparse_hash_map>
#include "treelet.h"
#include "Graph.h"
#include "GraphColoring.h"

class TreeletTable
{
    typedef uint64_t treelet_count_t;
    typedef google::sparse_hash_map<Treelet::treelet_t, treelet_count_t> table_t;

private:
    const Graph* graph;
    const long num_vertices;
    const GraphColoring* coloring;
    const int size;
    const TreeletTable* const* lower;

    table_t** counts;

    /// Fills a size-1 table
    void do_fill_1();

    /// Fills a table for sizes > 1
    void do_fill();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    void combine(long u, long v);

    /// Normalizes the counts of treelets rooted in @param u
    void normalize(long u);

public:
    TreeletTable(Graph* graph, GraphColoring* coloring, int size, const TreeletTable* const* lower = NULL);
    ~TreeletTable();

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void fill();
};


#endif //MOTIVO_TREELETTABLE_H
