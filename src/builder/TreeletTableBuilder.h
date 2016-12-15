//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELETTABLEBUILDER_H
#define MOTIVO_TREELETTABLEBUILDER_H

#include <sparsehash/sparse_hash_map>
#include <string>
#include <functional>
#include "../common/Treelet.h"
#include "../common/UndirectedGraph.h"
#include "../common/GraphColoring.h"
#include "../common/TreeletTable.h"
#include "../common/TreeletTableCollection.h"

class TreeletTableBuilder
{
private:
    struct TreeletHash
    {
        inline size_t operator() (const Treelet t) const
        {
            return t.hash();
        }
    };

    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, TreeletHash> table_t;

    const UndirectedGraph* graph;
    const GraphColoring* coloring;
    const unsigned int size;
    const TreeletTableCollection* lower;
    std::ostream* output;

    /// Fills a size-1 table
    void do_build_1(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to);

    /// Fills a table for sizes > 1
    void do_build(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to);

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    void combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);

    /// Normalizes the counts of treelets in @param counts
    void normalize(table_t& counts);

    ///Writes the content of the table to steam
    void write(const table_t& counts);

public:
    TreeletTableBuilder(UndirectedGraph* graph, GraphColoring* coloring, const unsigned int size, const TreeletTableCollection* lower, std::ostream* output)
            :  graph(graph), coloring(coloring), size(size), lower(lower), output(output) {};

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to);

};

#endif //MOTIVO_TREELETTABLEBUILDER_H
