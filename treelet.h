//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELET_H
#define MOTIVO_TREELET_H

#include <cstdint>
#include <cassert>

namespace Treelet
{
    /* Each treelet is represented as a bit string of 64 bits.
     * 0-31:  Structure of the treelet.
     * 32-47: Bitmask representing the treelet colors.
     * 48-55: Number of children isomorphic of the largest children.
     * 56-63: Size of the treelet
     *
     * Structure is encode as a DFS traversal, in binary.
     * 1 means that we entered a new vertex and 0 means we are leaving a vertex and its subtree
     * The bits needed for the hashcode to be unique are 2*size,
     * We can represent treelets up to size 16.
     * I.e: a star with 3 leavers is 11010100, a path with 4 nodes is 11110000,
     * a binary tree of height two is 11101001101000.
    */

    typedef uint64_t treelet_t;
    constexpr treelet_t invalid_treelet = 0; //note that 0 is not a valid treelet representation

    union __attribute__ ((__packed__)) treelet_t_union
    {
        treelet_t int_repr;

        struct __attribute__ ((__packed__))
        {
            uint32_t structure;
            uint16_t colors;
            uint8_t num_isomorphic_to_largest;
            uint8_t size;
        } fields;
    };

    ///Initializes a signleton treelet having color @param color
    uint64_t singleton(int color);

    ///Merges the @param t1 with @param t2
    ///@returns the merged treelet or invalid_treelet if @param t1 and @param t2 are not mergeable
    treelet_t merge(treelet_t t1, treelet_t t2);

    uint8_t normalization_factor(treelet_t t);

}
#endif //MOTIVO_TREELET_H
