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

#ifndef MOTIVO_TREELETTABLE_H
#define MOTIVO_TREELETTABLE_H

#include <cstdint>
#include <string>
#include "Treelet.h"
#include "TreeletStructureSelector.h"
#include "../random/Random.h"
#include "../graph/UndirectedGraph.h"
#include "../random/AliasMethodSampler.h"
#include "../platform/platform.h"
#include "../io/CompressedRecordFile.h"
#include "../random/RangeSampler.h"

class TreeletTable
{
public:
    typedef uint128_t treelet_count_t;

    struct [[gnu::packed]] treelet_count_pair
    {
        Treelet treelet;
        treelet_count_t count;
    };

    static_assert( sizeof(treelet_count_pair) ==  sizeof(Treelet) + sizeof(treelet_count_t), "treelet_count_pair is not packed" );

    struct [[gnu::packed]]
#ifdef MOTIVO_MAY_ALIAS
            [[gnu::may_alias]]
#endif
    treelet_count_pair_maybe_alias
    {
        Treelet treelet;
        treelet_count_t count;
    };

    static_assert( alignof(treelet_count_pair_maybe_alias) == 1, "treelet_count_pair_maybe_alias is not 1-byte aligned" );
    static_assert( sizeof(treelet_count_pair_maybe_alias) ==  sizeof(Treelet) + sizeof(treelet_count_t), "treelet_count_pair_maybe_alias is not packed" );

#ifdef MOTIVO_MAY_ALIAS
    static constexpr bool may_alias = true;
#else
    static constexpr bool may_alias = false;
#endif

    class const_iterator
    {
    friend class TreeletTable;

    private:
        bool owner = true; //who owns the record?
        Record<const treelet_count_pair_maybe_alias> record;
        const treelet_count_pair_maybe_alias* position;
        explicit const_iterator(Record<const treelet_count_pair_maybe_alias> record) : record(record)
        {
            position=record.begin();
            if(position)
                position++;
        }

        const_iterator(Record<const treelet_count_pair_maybe_alias> record, const treelet_count_pair_maybe_alias* position) : record(record), position(position) {};

    public:
        const_iterator(const_iterator&) = delete; //copy constructor
        const_iterator& operator=(const const_iterator& other) = delete; //assignment

        const_iterator(const_iterator&& other) noexcept : record(other.record), position(other.position) //move constructor
        {
            other.owner = false;
        }

        ~const_iterator() { if(owner) record.free(); }

        const_iterator& operator++() { position++; return *this; };
        const Treelet treelet() const { return position->treelet; };
        treelet_count_t count() const { return position->count - (position-1)->count; }
        bool is_over() const { return position>=record.end(); }
    };

private:
    UndirectedGraph::vertex_t num_vertices;
    BaseRecordSource<const treelet_count_pair_maybe_alias>* reader;
    AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>* root_sampler;

    static const TreeletTable::treelet_count_pair_maybe_alias* treelet_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin, const TreeletTable::treelet_count_pair_maybe_alias *end, const Treelet &treelet);
    static const TreeletTable::treelet_count_pair_maybe_alias* count_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin, const TreeletTable::treelet_count_pair_maybe_alias *end, TreeletTable::treelet_count_t count);


public:
    TreeletTable(const TreeletTable&) = delete;
    void operator=(const TreeletTable&) = delete;

    ///Loads a table stored with the given @param basename.
    explicit TreeletTable(BaseRecordSource<const treelet_count_pair_maybe_alias>* record_source);

    ~TreeletTable();

    ///Loads the associated root sampler
    void load_root_sampler(const std::string& filename);

    ///@returns the number of vertices written in the table
    UndirectedGraph::vertex_t number_of_vertices() { return num_vertices; };

    ///@returns a root r chosen at random with probability proportional to the number of treelets rooted in r
    ///the associated root sampler must be loaded
    UndirectedGraph::vertex_t get_random_root(Random* rng) const;

    ///@returns the Treelet number @param no (counting from 0) among the ones stored in @param root
    const Treelet get_treelet_no(UndirectedGraph::vertex_t root, treelet_count_t no) const;

    ///@returns a Treelet chosen uniformly at random from all the treelts rooted in @param root
    const Treelet get_random_treelet(UndirectedGraph::vertex_t root, Random *rng);

    ///@returns the number of occurrences of @param treelet rooted in @param u, as stored in the table.
    treelet_count_t get_count(UndirectedGraph::vertex_t u, Treelet treelet) const;


    RangeSampler<treelet_count_t>* build_range_sampler(UndirectedGraph::vertex_t u, const TreeletStructureSelector* selector);

    ///@returns a constant iterator that iterates through all the stored treelets for vertex @param u.
    ///The iterator initially points to the first treelet of @param u.
    inline const_iterator begin(const UndirectedGraph::vertex_t u)
    {
        assert(u<num_vertices);
        return TreeletTable::const_iterator(reader->get_record(u));
    }

    const_iterator begin(UndirectedGraph::vertex_t u, Treelet treelet);
};


#endif //MOTIVO_TREELETTABLE_H
