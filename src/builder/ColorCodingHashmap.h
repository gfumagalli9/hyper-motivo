//
// Created by steven on 12/28/18.
//

#ifndef MOTIVO_COLORCODINGHASHMAP_H
#define MOTIVO_COLORCODINGHASHMAP_H

#include "config.h"

#ifdef MOTIVO_DENSE_HASHMAP
    #include <sparsehash/dense_hash_map>
#else
    #include <sparsehash/sparse_hash_map>
#endif

#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"

class ColorCodingHashmap
{
private:
#ifdef MOTIVO_DENSE_HASHMAP
    typedef google::dense_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#else
    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, Treelet::TreeletHash> table_t;
#endif

    table_t hashmap;

public:
    typedef table_t::const_iterator const_iterator;

#ifdef MOTIVO_DENSE_HASHMAP
    ColorCodingHashmap()
    {
        hashmap.set_empty_key(Treelet::invalid_treelet);
    }
#else
    ColorCodingHashmap() = default;
#endif

    inline TreeletTable::treelet_count_t& operator[](const Treelet& k)
    {
        return hashmap[k];
    }

    inline void clear()
    {
        hashmap.clear();
    }

    inline const_iterator begin() const
    {
        return hashmap.begin();
    }

    inline const_iterator end() const
    {
        return hashmap.end();
    }

    //The maximum number M_i of possible colored treelets of size i is: N_i * (16 choose i)
    //where N_i = nhe number of rooted treelets with i nodes.
    //From https://oeis.org/A000081, N_i for i=1,...,16 is: 1, 1, 2, 4, 9, 20, 48, 115, 286, 719, 1842, 4766, 12486, 32973, 87811, 235381
    //The maximum possible size of the hashtables is the sum of:
    //Colored treelets: M_1 + ...+ M_16 = 40576023
    //Uncolored treelets: N_1 + ... + N_16 = 376464
    //1 Invalid treelet
    //TOTAL: 40952488 < 2^26
    inline unsigned long size() const
    {
        return hashmap.size();
    }
};

#endif //MOTIVO_COLORCODINGHASHMAP_H
