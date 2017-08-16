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
#include "ConcurrentFIFO.h"
#include "SpookyHash.h"
#include "BaseSequencer.h"

#ifdef MOTIVO_MULTITHREAD
    #include <mutex>
#endif

class TreeletTableBuilder
{
private:
    struct TreeletHash
    {
        inline size_t operator() [[gnu::hot,gnu::flatten]] (const Treelet t) const
        {
            return SpookyHash::Hash64(&t, sizeof(t), 0);
        }
    };

    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, TreeletHash> table_t;

    const UndirectedGraph* graph;
    const GraphColoring* coloring;
    const unsigned int size;
    const TreeletTableCollection* lower;
    std::ostream* output;
    BaseSequencer* const sequencer;
    const bool store_0_only;
    const unsigned int number_of_threads;

#ifdef MOTIVO_MULTITHREAD
    ConcurrentFIFO< std::pair<char*, std::size_t>* > write_queue;

    /// Fills a table for sizes > 1
    void do_build_mt [[gnu::hot,gnu::flatten]]();

    ///Writing thread entry point
    void writer_loop();
#endif

    /// Fills a size-1 table
    void do_build_1_st [[gnu::hot,gnu::flatten]]();

    /// Fills a table for sizes > 1
    void do_build_st [[gnu::hot,gnu::flatten]]();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    inline void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);

    inline std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const UndirectedGraph::vertex_t u, const table_t &table);

public:
    TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
                        const TreeletTableCollection* lower, std::ostream* output, BaseSequencer* const sequencer,
                        const bool store_0_only=false, const unsigned int num_threads=1);

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build();
};

#endif //MOTIVO_TREELETTABLEBUILDER_H
