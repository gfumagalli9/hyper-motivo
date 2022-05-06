// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef MOTIVO_COLORCODINGHASHMAP_H
#define MOTIVO_COLORCODINGHASHMAP_H

#include "config.h"

#ifdef MOTIVO_DENSE_HASHMAP
    #include <dense_hash_map>
#else
    #include <sparse_hash_map>
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
        hashmap.set_empty_key(invalid_treelet);
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
