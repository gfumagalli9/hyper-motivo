//
// Created by steven on 11/20/16.
//

#ifndef MOTIVO_TREELETTABLE_H
#define MOTIVO_TREELETTABLE_H

#include <cstdint>
#include <string>
#include "Treelet.h"
#include "Random.h"
#include "UndirectedGraph.h"
#include "AliasMethodSampler.h"
#include "../platform/platform.h"
#include "CompressedRecordFile.h"

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

private:
    struct [[gnu::packed]]
#ifdef MOTIVO_MAY_ALIAS
            [[gnu::may_alias]]
#endif
    treelet_count_pair_maybe_alias
    {
        Treelet treelet;
        treelet_count_t count;
    };

    static_assert( sizeof(treelet_count_pair_maybe_alias) ==  sizeof(Treelet) + sizeof(treelet_count_t), "treelet_count_pair_maybe_alias is not packed" );

public:

    class const_iterator
    {
    friend class TreeletTable;

    private:
        bool owner = true; //who owns the record?
        CompressedRecord<treelet_count_pair_maybe_alias> record;
        const treelet_count_pair_maybe_alias* position;
        const_iterator(CompressedRecord<treelet_count_pair_maybe_alias> record) : const_iterator(record, record.begin()) {};
        const_iterator(CompressedRecord<treelet_count_pair_maybe_alias> record, const treelet_count_pair_maybe_alias* position) : record(record), position(position?(position+1): nullptr) {};

    public:
        const_iterator(const_iterator&) = delete; //copy constructor
        const_iterator& operator=(const const_iterator& other) = delete; //assignment

        const_iterator(const_iterator&& other) : record(other.record) { other.owner = false; }; //move constructor

        ~const_iterator() { if(owner) record.free(); }
        const_iterator& operator++() { position++; return *this; };
        const Treelet treelet() const { return position->treelet; };
        treelet_count_t count() const { return position->count - (position-1)->count; }
        bool is_over() const { return position>=record.end(); }
    };

private:
    UndirectedGraph::vertex_t num_vertices;
    CompressedRecordFileReader reader;
    AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>* root_sampler;

    static const TreeletTable::treelet_count_pair_maybe_alias* treelet_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin, const TreeletTable::treelet_count_pair_maybe_alias *end, const Treelet &treelet);
    static const TreeletTable::treelet_count_pair_maybe_alias* count_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin, const TreeletTable::treelet_count_pair_maybe_alias *end, TreeletTable::treelet_count_t count);

    TreeletTable(const TreeletTable&) = delete;
    void operator=(const TreeletTable&) = delete;

public:
    ///Loads a table stored with the given @param basename.
    ///If @param load_root_sampler is true, it loads the associated root sampler, if available.
    TreeletTable(const std::string& basename, const bool load_root_sampler=true);
    ~TreeletTable();

    ///@returns a root r chosen at random with probability proportional to the number of treelets  rooted in r
    UndirectedGraph::vertex_t get_random_root(Random* rng) const;

    ///@returns a Treelet  chosen uniformly at random from all the treelts roote in @param root
    const Treelet get_random_treelet(UndirectedGraph::vertex_t root, Random *rng);

    ///@returns the number of occurrences of @param treelet rooted in @param u, as stored in the table.
    treelet_count_t get_count(const UndirectedGraph::vertex_t u, const Treelet treelet);

    ///@returns a costant iterator that iterates through all the stored treelets for vertex @param u.
    ///The iterator initially points to the first treelet of @param u.
    inline const_iterator begin(const UndirectedGraph::vertex_t u)
    {
        assert(u<num_vertices);
        return TreeletTable::const_iterator(reader.get_record<treelet_count_pair_maybe_alias>(u));
    }

    const_iterator begin(const UndirectedGraph::vertex_t u, const Treelet treelet);

};


#endif //MOTIVO_TREELETTABLE_H
