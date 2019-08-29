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

#ifndef MOTIVO_COLORCODINGBUILDER_H
#define MOTIVO_COLORCODINGBUILDER_H

#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"


class ColorCodingBuilder
{
private:
    const unsigned int size;
    const TreeletTableCollection* const lower;
    const TreeletStructureSelector* const selector;

public:
    ColorCodingBuilder(const unsigned int size, const TreeletTableCollection* lower, const TreeletStructureSelector* const selector)  : size(size), lower(lower), selector(selector)
    {
        if(size==0)
            throw std::runtime_error("Invalid size");
    }


    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    /// Thread safe as long as @param counts is not accessed while this method is running
    template<typename T> inline void combine [[gnu::hot]] (const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, T& counts)
    {
        for(unsigned int size1=1; size1<size; size1++)
        {
            TreeletTable* u_table = lower->get_table(size1);
            TreeletTable* v_table = lower->get_table(size-size1);

            for(TreeletTable::const_iterator u_it = u_table->begin(u); !u_it.is_over(); ++u_it)
            {
                const Treelet t1 = u_it.treelet();
                assert(t1.is_valid());
                assert(u_it.count() != 0);

                for(TreeletTable::const_iterator v_it = v_table->begin(v); !v_it.is_over(); ++v_it)
                {
                    const Treelet t2 = v_it.treelet();
                    assert(t2.is_valid());
                    assert(v_it.count() != 0);

                    Treelet merged = t1.merge(t2);
                    if(merged.is_valid() && (!selector || selector->is_included(merged.get_structure())))
                    {
                        TreeletTable::treelet_count_t &count = counts[merged];
                        TreeletTable::treelet_count_t tmp;
                        safe_mul(u_it.count(), v_it.count(), &tmp);
                        safe_add(count, tmp, &count);
                    }
                    else if(merged == invalid_merge_structure)
                        break; //All the remaining treelets t2 will have a structure that is too small.
                }
            }
        }
    }

    template<typename T> std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const UndirectedGraph::vertex_t u, const T &table)
    {
        std::pair<char*, std::size_t> result;
        result.second = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + table.size() * sizeof(TreeletTable::treelet_count_pair);
        result.first = new char[result.second];

        //Prevent alignment issues (size might get copied to unaligned memory)
        memcpy(result.first, &u, sizeof(UndirectedGraph::vertex_t));
        uint64_t size = table.size();
        memcpy(result.first + sizeof(UndirectedGraph::vertex_t), &size, sizeof(uint64_t));

        //Make sure array is properly aligned
        static_assert( (sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t)) % alignof(TreeletTable::treelet_count_pair) == 0, "treelet_count_pair not aligned in buffer" );
        auto counts = new(result.first+sizeof(UndirectedGraph::vertex_t)+sizeof(uint64_t)) TreeletTable::treelet_count_pair[size];
        TreeletTable::treelet_count_t i=0;
        typename T::const_iterator u_it = table.begin();
        while(u_it != table.end())
        {
            assert(u_it->second > 0);
            assert(u_it->second % u_it->first.normalization_factor() == 0);

            counts[i].treelet = u_it->first;
            counts[i].count = u_it->second / counts[i].treelet.normalization_factor();

            u_it++;
            i++;
        }

        std::sort(counts, counts+table.size(), [](const TreeletTable::treelet_count_pair& tc1, const TreeletTable::treelet_count_pair& tc2) { return tc1.treelet < tc2.treelet; } );

        return result;
    }

};

#endif //MOTIVO_COLORCODINGBUILDER_H