//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELETTABLEBUILDER_H
#define MOTIVO_TREELETTABLEBUILDER_H

#include <sparsehash/sparse_hash_map>
#include <string>
#include <functional>
#include "config.h"
#include "../common/Treelet.h"
#include "../common/UndirectedGraph.h"
#include "../common/GraphColoring.h"
#include "../common/TreeletTable.h"
#include "../common/TreeletTableCollection.h"

#ifdef MOTIVO_MULTITHREAD
    #include <mutex>
#endif

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
    const UndirectedGraph::vertex_t from;
    const UndirectedGraph::vertex_t to;
    std::ostream* output;

#ifdef MOTIVO_MULTITHREAD
    constexpr static unsigned int thread_buffer_size = 1000;
    std::mutex write_mutex;

     /// Fills a size-1 table
    void do_build_1_mt [[gnu::hot]](std::atomic<UndirectedGraph::vertex_t> *atomic_cnt);

    /// Fills a table for sizes > 1
    void do_build_mt [[gnu::hot]](std::atomic<UndirectedGraph::vertex_t> *atomic_cnt);
#endif

    /// Fills a size-1 table
    void do_build_1_st [[gnu::hot]]();

    /// Fills a table for sizes > 1
    void do_build_st [[gnu::hot]]();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);

    /// Normalizes the counts of treelets in @param counts
    void normalize [[gnu::hot]] (table_t& counts);

    ///Writes the content of the table to steam
    void write [[gnu::hot]](const UndirectedGraph::vertex_t vertex, const table_t &counts);

public:
    TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
                        const TreeletTableCollection* lower,  const UndirectedGraph::vertex_t from,
                        const UndirectedGraph::vertex_t to, std::ostream* output)
            :  graph(graph), coloring(coloring), size(size), lower(lower), from(from), to(to), output(output) {};

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build(unsigned int nthreads=1);
};

#endif //MOTIVO_TREELETTABLEBUILDER_H
