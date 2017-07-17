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

    class const_iterator
    {
    friend class TreeletTable;

    private:
        const treelet_count_pair* position;
        const_iterator(const treelet_count_pair* position) : position(position) {};

    public:
        const_iterator(const const_iterator& iterator) { position = iterator.position; };
        const_iterator& operator=(const const_iterator& iterator) { position = iterator.position; return *this; };
        const_iterator& operator++() { position++; return *this; };
        const_iterator operator++(int) { return position++; };
        const Treelet& treelet() const { return position->treelet; };
        treelet_count_t count() const { return position->count - (position-1)->count; }
        bool operator==(const const_iterator& iterator) const { return position == iterator.position; }
        bool operator!=(const const_iterator& iterator) const { return position != iterator.position; }
    };

private:
    UndirectedGraph::vertex_t num_vertices;
    uint64_t* offsets;
    treelet_count_pair* data;
    FILE* data_fd;
    FILE* offsets_fd;
    AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>* root_sampler;

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
    const Treelet& get_random_treelet(UndirectedGraph::vertex_t root, Random *rng) const;

    ///@returns the number of occurrences of @param treelet rooted in @param u, as stored in the table.
    treelet_count_t get_count(const UndirectedGraph::vertex_t u, const Treelet treelet) const;

    ///@returns a costant iterator that iterates through all the stored treelets for vertex @param u.
    ///The iterator initially points to the first treelet of @param u.
    inline const_iterator begin(const UndirectedGraph::vertex_t u) const
    {
        assert(u<num_vertices);
        return TreeletTable::const_iterator( data + offsets[u] + 1 );
    }

    ///@returns an iterator pointing to after the last treelet stored for vertex @param u.
    inline const_iterator end(const UndirectedGraph::vertex_t u) const
    {
        assert(u<num_vertices);
        return TreeletTable::const_iterator( data + offsets[u+1] );
    }

    const_iterator begin(const UndirectedGraph::vertex_t u, const Treelet treelet) const;

};


#endif //MOTIVO_TREELETTABLE_H
