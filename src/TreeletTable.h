//
// Created by steven on 11/20/16.
//

#ifndef MOTIVO_TREELETTABLE_H
#define MOTIVO_TREELETTABLE_H

#include <cstdint>
#include <string>
#include "treelet.h"
#include "cmph.h"

class TreeletTable
{
public:
    typedef uint64_t treelet_count_t;

    struct treelet_count_pair
    {
        Treelet::treelet_t treelet;
        treelet_count_t count;
    };
    static_assert( sizeof(treelet_count_pair) ==  sizeof(Treelet::treelet_t) + sizeof(treelet_count_t), "treelet_count_pair is not packed" );

    class const_iterator
    {
    private:
        treelet_count_pair* position;

    public:
        const_iterator(treelet_count_pair* position) : position(position) {};
        const_iterator(const const_iterator& iterator) { position = iterator.position; };
        const_iterator& operator=(const const_iterator& iterator) { position = iterator.position; return *this; };
        const_iterator& operator++() { position++; return *this; };
        const_iterator operator++(int) { return position++; };
        const treelet_count_pair& operator*() const { return *position; };
        const treelet_count_pair* operator->() const { return position; }
        bool operator==(const const_iterator& iterator) const { return position == iterator.position; }
        bool operator!=(const const_iterator& iterator) const { return position != iterator.position; }
    };

private:
    long size;
    uint64_t* offsets;
    treelet_count_pair* data;
    cmph_t** hashes;
    FILE* data_fd;
    FILE* offsets_fd;

public:
    ///Loads a table stored with the given @param basename.
    ///Also loads the associated perfect hash function, if available.
    TreeletTable(const std::string& basename);
    ~TreeletTable();

    ///@returns the number of occurrences of @param treelet rooted in @param u, as stored in the table.
    treelet_count_t get_count(const long u, const Treelet::treelet_t treelet) const;

    ///@returns a costant iterator that iterates through all the stored treelets for vertex @param u.
    ///The iterator initially points to the first treelet of @param u.
    const_iterator begin(const long u) const;

    ///@returns an iterator pointing to after the last treelet stored for vertex @param u.
    const_iterator end(const long u) const;
};


#endif //MOTIVO_TREELETTABLE_H
