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
public:
    typedef void (*progress_callback_t)(UndirectedGraph::vertex_t);

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
    progress_callback_t progress_callback;
    UndirectedGraph::vertex_t progress_interval;

#ifdef MOTIVO_MULTITHREAD
    constexpr static const unsigned int thread_buffer_size = 1000;
    std::mutex write_mutex;

     /// Fills a size-1 table
    void do_build_1_mt [[gnu::hot,gnu::flatten]](std::atomic<UndirectedGraph::vertex_t> *atomic_cnt);

    /// Fills a table for sizes > 1
    void do_build_mt [[gnu::hot,gnu::flatten]](std::atomic<UndirectedGraph::vertex_t> *atomic_cnt);
#endif

    /// Fills a size-1 table
    void do_build_1_st [[gnu::hot,gnu::flatten]]();

    /// Fills a table for sizes > 1
    void do_build_st [[gnu::hot,gnu::flatten]]();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    inline void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);

    inline TreeletTable::treelet_count_pair* to_normalized_sorted_array [[gnu::hot]] (const table_t &table);

    ///Writes the content of the table to steam
    void write_one [[gnu::hot]] (const UndirectedGraph::vertex_t vertex, const TreeletTable::treelet_count_pair *counts, const TreeletTable::treelet_count_t ntreelets);

    inline void report_progress(UndirectedGraph::vertex_t next_vertex)
    {
        if(progress_callback!= nullptr && (next_vertex%progress_interval)==0)
            (*progress_callback)(next_vertex);
    }

public:
    TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
                        const TreeletTableCollection* lower,  const UndirectedGraph::vertex_t from,
                        const UndirectedGraph::vertex_t to, std::ostream* output)
            :  graph(graph), coloring(coloring), size(size), lower(lower), from(from), to(to), output(output),
               progress_callback(nullptr) {};

    void set_progress_callback(progress_callback_t pc, UndirectedGraph::vertex_t pi)
    {
        progress_callback = pc;
        progress_interval = pi;
    }

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build(unsigned int nthreads=1);
};

#endif //MOTIVO_TREELETTABLEBUILDER_H
