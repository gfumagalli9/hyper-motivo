//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELET_H
#define MOTIVO_TREELET_H

#include <cstdint>
#include <cassert>
#include <utility>
#include "platform.h"


//The following classsis already packed.
//See: https://en.wikipedia.org/wiki/Data_structure_alignment#Typical_alignment_of_C_structs_on_x86
//#pragma test(push,1)
class [[gnu::packed]] Treelet
{
    /* Each treelet is represented as a bit string of 64 bits.
     * 0-31:  Structure of the treelet.
     * 32-47: Bitmask representing the treelet colors.
     * 48-55: Number of children isomorphic of the largest children.
     * 56-63: Size of the treelet
     *
     * Structure is encode as a DFS traversal, in binary.
     * 1 means that we entered a new vertex and 0 means we are leaving a vertex and its subtree.
     * The first bit is always 1 and it is not stored. Bits are left-aligned.
     * We can represent treelets up to size 16 (using 31 bits).
     * I.e: a star with 3 leavers is 1010100 followed by 25 zeros, a path with 4 nodes is 1110000 followed by 26 zeros,
     * a binary tree of height two is 1101001101000 followed by 19 zeros.
    */
public:
    typedef uint32_t treelet_structure_t;
    typedef uint16_t treelet_colors_t;
    constexpr static int treelet_structure_bits = 32;
    constexpr static treelet_structure_t treelet_structure_highest_bit = 1u<<(treelet_structure_bits-1);
    constexpr static treelet_structure_t invalid_structure = 0xFFFFFFFF;
    constexpr static treelet_structure_t singleton_structure = 0;


private:
    treelet_structure_t structure;
    uint16_t colors;
    //uint8_t children_isomorphic_to_largest;
    //uint8_t size;

    Treelet(treelet_structure_t structure, uint16_t colors=0) : structure(structure), colors(colors)
    {};

public:
    Treelet() = default;

    const static Treelet invalid_treelet; //A generic invalid treelet representation
    const static Treelet invalid_merge_colors; //Merge failed due to intersecting colors
    const static Treelet invalid_merge_structure; //Merge failed due to wrong structure order

    ///@returns the number of vertices of the treelet
    inline unsigned int number_of_vertices() const { return static_cast<unsigned  int>(popcount32(structure)+1); }

    ///@returns true iff the represented treelet is invalid, e.g., due to a failed merge
    inline bool is_valid() const { return structure!=invalid_structure; }

    ///@returns true iff the treelet is colored
    inline bool is_colored() const { return colors !=0; }

    ///@returns true iff the treelet is colored
    inline treelet_colors_t get_colors() const { return colors; }

    ///Initializes a signleton treelet having color @param color
    inline static Treelet singleton(const uint8_t color) { return Treelet(singleton_structure, static_cast<uint16_t>(1 << color)); }

    ///Merges the the current treelet with @param other
    ///@returns the merged treelet or an invalid treelet if the current treelet and @param t2 are not mergeable
    Treelet merge(const Treelet other) const;

    ///@returns the number of times a treelet will be overcounted when merging using "merge"
    uint8_t normalization_factor() const;

    ///@returns an opaque value representing the structure of the treelet
    inline treelet_structure_t get_structure() const { return structure; }

    //FIXME: Better hash?
    inline std::size_t hash() const { return std::hash<uint64_t>{}( (static_cast<uint64_t>(structure)<<16) | colors); }

    Treelet split_child() const;

    Treelet complement(Treelet t2) const;

    inline bool operator==(const Treelet& other) const { return structure==other.structure && colors==other.colors; }
    inline bool operator<(const Treelet& other) const { return (structure > other.structure) || (structure == other.structure && colors < other.colors); }
    inline bool operator<=(const Treelet& other) const { return (structure > other.structure) || (structure == other.structure && colors <= other.colors); }
};
//#pragma test(pop)

static_assert(sizeof(Treelet) == 6, "treelet_t_union is not packed in 8 bytes");


#endif //MOTIVO_TREELET_H
