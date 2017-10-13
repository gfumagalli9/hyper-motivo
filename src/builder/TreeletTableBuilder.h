//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELETTABLEBUILDER_H
#define MOTIVO_TREELETTABLEBUILDER_H

#include <sparsehash/sparse_hash_map>
#include <sparsehash/dense_hash_map>
#include <string>
#include <functional>
#include "config.h"
#include "../common/treelets/Treelet.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/GraphColoring.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/sequencer/BaseSequencer.h"
#include "../common/io/ConcurrentFIFO.h"
#include "../common/io/ConcurrentWriter.h"

#ifdef MOTIVO_MULTITHREAD
    #include <mutex>
#endif

class TreeletTableBuilder
{
public:
    typedef BaseSequencer<UndirectedGraph::vertex_t> sequencer_t;

private:
    struct TreeletHash
    {
        inline size_t operator() [[gnu::hot,gnu::flatten]] (const Treelet t) const
        {
            uint64_t key=0;
            memcpy(&key, &t, sizeof(Treelet));

            //MurmurHash3 finalizer by Austin Appleby (public domain)
            key ^= key >> 33;
            key *= 0xff51afd7ed558ccd;
            key ^= key >> 33;
            key *= 0xc4ceb9fe1a85ec53;
            key ^= key >> 33;

            return key;
        }
    };

#ifdef MOTIVO_DENSE_HASHMAP
    typedef google::dense_hash_map<Treelet, TreeletTable::treelet_count_t, TreeletHash> table_t;
#define MOTIVO_INIT_HASHMAP(hm) do { (hm).set_empty_key(Treelet::invalid_treelet); } while(false)
#else
    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, TreeletHash> table_t;
#define MOTIVO_INIT_HASHMAP(hm) do {} while(false)
#endif

    const UndirectedGraph* graph;
    const GraphColoring* coloring;
    const unsigned int size;
    const TreeletTableCollection* lower;
    std::ostream* output;
    sequencer_t* const sequencer;
    const unsigned int number_of_threads;

    const bool store_0_only;
    bool selective;
    uint64_t selective_num = 0;
    uint64_t selective_capacity = 0;
    Treelet* selective_treelets = nullptr;

    /// Fills a table for sizes > 1
    void do_build_mt [[gnu::hot,gnu::flatten]](ConcurrentWriter *writer);

    /// Fills a size-1 table
    void do_build_1_st [[gnu::hot,gnu::flatten]]();

    /// Fills a table for sizes > 1
    void do_build_st [[gnu::hot,gnu::flatten]]();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    inline void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts);

    inline std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const UndirectedGraph::vertex_t u, const table_t &table);

    inline bool should_count(Treelet t);

public:
    TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
                        const TreeletTableCollection* lower, std::ostream* output, sequencer_t* const sequencer,
                        const unsigned int num_threads=1, const bool store_0_only=false, bool selective=false);

    ~TreeletTableBuilder();

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build();

    bool add_selective_treelet(const Treelet treelet);
};

#endif //MOTIVO_TREELETTABLEBUILDER_H
