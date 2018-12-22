//
// Created by steven on 12/21/18.
//

#ifndef MOTIVO_COLORCODINGBUILDER_H
#define MOTIVO_COLORCODINGBUILDER_H

#include <sparsehash/sparse_hash_map>
#include <sparsehash/dense_hash_map>
#include <string>
#include <functional>
#include "config.h"
#include "../common/treelets/Treelet.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletSelector.h"

class ColorCodingBuilder
{
private:
    struct TreeletHash {
        inline size_t operator()[[gnu::hot,gnu::flatten]] (const Treelet t) const
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

public:
#ifdef MOTIVO_DENSE_HASHMAP
    typedef google::dense_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#define BUILDER_INIT_HASHMAP(hm) do { (hm).set_empty_key(Treelet::invalid_treelet); } while(false)
#else
    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#define BUILDER_INIT_HASHMAP(hm) do {} while(false)
#endif


private:
    const unsigned int size;
    const TreeletTableCollection* lower;
    TreeletSelector* selector;

public:
    ColorCodingBuilder(unsigned int size, const TreeletTableCollection* lower, TreeletSelector* selector);


    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    /// Thread safe as long as @param counts is not accessed while this method is running
    void combine [[gnu::hot]] (UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t v, table_t& counts);

    std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](UndirectedGraph::vertex_t u, const table_t &table);
};


#endif //MOTIVO_COLORCODINGBUILDER_H
