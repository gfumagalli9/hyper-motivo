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

#include <cstdint>
#include <climits>
#include <algorithm>
#include "Treelet.h"

Treelet Treelet::merge(const Treelet other) const
{
    if(colors & other.colors) //colors intersect
        return invalid_merge_colors;

    const unsigned int other_size = other.number_of_vertices();
    treelet_structure_t new_structure = treelet_structure_highest_bit + (other.structure >> 1u) + (structure >> (2*other_size));

    //Let x be the first child of this. Let |t| denote the num_vertices of t.
    //If x and t2 coincide then there the first 2*max(|x|,|t2|) bits of the structure of this and t2 coincide
    //If x and t2 differ then there at least one bit in the first 2*min(|x|,|t2|) bits of the structure of this and t2 differs
    if( (structure ^ new_structure) >> (treelet_structure_bits - 2*other_size) ) //True iff x and t2 differ.
    {
        if( structure > new_structure ) //The first differing bit determines the result
            return invalid_merge_structure;

        return Treelet(new_structure, colors | other.colors);
    }

    return Treelet(new_structure, colors | other.colors);
}

Treelet Treelet::split_child() const
{
    const int child_bits = leftmost_bit_tie1(structure);
    treelet_structure_t child_structure =  (structure<<1) & (0xFFFFFFFFu << (treelet_structure_bits-child_bits + 1));
    return Treelet(child_structure);
}

Treelet Treelet::complement(Treelet t2) const
{
    assert( (t2.colors & ~colors) == 0 );
    assert( (((treelet_structure_highest_bit | (t2.structure>>1)) ^ structure) >> (treelet_structure_bits- 2*t2.number_of_vertices())) == 0);

    return Treelet(structure << (2*t2.number_of_vertices()), static_cast<treelet_colors_t>(colors & (~t2.colors)) );
}

uint8_t Treelet::normalization_factor() const
{
    if(structure == singleton_structure)
        return 1;

    const int child_bits = leftmost_bit_tie1(structure);
    treelet_structure_t child_mask =  0xFFFFFFFFu << (treelet_structure_bits-child_bits);
    treelet_structure_t child_structure = structure & child_mask;

    uint8_t num_occurrences = 1;
    while( (((structure << (child_bits*num_occurrences)) ^ child_structure) & child_mask) == 0 )
        num_occurrences++;

    return num_occurrences;
}

Treelet Treelet::canonical_rooting() const
{
    assert(is_valid());

    //Find the center(s) of the treelet
    unsigned int parents[16] = {0};
    unsigned int current = 0;
    unsigned int num_vertices = 1;

    unsigned int depth=0; //current depth in the visit
    unsigned int max_depth=0; //maximum depth in the current subtree of the root
    unsigned int deepest_leaf=0; //the leaf corresponding to max_depth

    unsigned int depth_subtree_1=0; //the height of the highest subtree of the root so far
    unsigned int depth_subtree_2=0; //the height of the second highest subtree of the root so far
    unsigned int deepest_leaf_subtree_1=0; //the leaf corresponding to depth_subtree_1

    unsigned int subtree_bit_start[16] = {0}; //i-th entry = index of the first of the subtree starting at vertex i excluding the leading 1
    unsigned int subtree_bit_end[16] = {0}; //i-th entry = index of the last of the subtree starting at vertex i (i.e., the "0" leaving i)

    unsigned int index=0;
    treelet_structure_t remaining = structure;
    while(true)
    {
        if(remaining & treelet_structure_highest_bit) //new edge
        {
            parents[num_vertices]=current;
            subtree_bit_start[num_vertices]=index+1;
            current=num_vertices;
            num_vertices++;
            depth++;
        }
        else
        {
            subtree_bit_end[current]=index;

            if(current==0)
                break;

            if(depth>max_depth)
            {
                max_depth = depth;
                deepest_leaf = current;
            }

            current=parents[current];
            depth--;

            if(current==0)
            {
                if(max_depth>=depth_subtree_1)
                {
                    depth_subtree_2=depth_subtree_1;

                    depth_subtree_1=max_depth;
                    deepest_leaf_subtree_1=deepest_leaf;
                }
                else if(max_depth>depth_subtree_2)
                    depth_subtree_2=max_depth;

                max_depth=0;
                deepest_leaf=0;
            }
        }

        remaining<<=1u;
        index++;
    }

    unsigned int deepest_center = deepest_leaf_subtree_1;
    for(unsigned int i=(depth_subtree_1 + depth_subtree_2)/2; i>0; i--)
        deepest_center = parents[deepest_center];



    Treelet rerooting_1 = reroot(deepest_center, parents, subtree_bit_start, subtree_bit_end);
    if((depth_subtree_1 + depth_subtree_2)%2==0) //there is only one center
        return rerooting_1;
    else //2 centers
    {
        //the second center is the parent of deepest_center
        Treelet rerooting_2 = reroot(parents[deepest_center], parents, subtree_bit_start, subtree_bit_end);
        return (rerooting_1 < rerooting_2)?rerooting_1:rerooting_2;
    }
}

//Selects the bits in positions [from, from+len) of src and returns them in positions [pos, pos+len)
#define STRUCTURE_BITSELECT(src, from, len, pos) ( ( ( (src) >> (treelet_structure_bits - ((from)+(len)) ) ) << (treelet_structure_bits - (len) ) ) >> (pos) )


Treelet Treelet::reroot(unsigned int new_root, const unsigned int* parents, const unsigned int* subtree_bit_start, const unsigned int* subtree_bit_end) const
{
    assert(is_valid());
    assert(new_root < number_of_vertices());

    if(new_root==0) //nothing to do
        return *this;

    //Generate structure corresponding to a dfs visit from new_root

    //Copy the subtree rooted at new_root
    treelet_structure_t dfs_structure=0;
    unsigned int index = subtree_bit_end[new_root] - subtree_bit_start[new_root];
    if(index>0)
        dfs_structure = STRUCTURE_BITSELECT(structure, subtree_bit_start[new_root], index, 0);

    //Handle the parents of new_root
    for(unsigned int completed = new_root; completed!=0; completed=parents[completed])
    {
        unsigned int parent = parents[completed];
        dfs_structure |= (treelet_structure_highest_bit >> index);
        index++;

        assert(subtree_bit_start[parent]<subtree_bit_start[completed]);
        assert(subtree_bit_end[parent]>subtree_bit_end[completed]);

        unsigned int len = subtree_bit_start[completed] - subtree_bit_start[parent] - 1;
        if(len>0)
        {
            dfs_structure |= STRUCTURE_BITSELECT(structure, subtree_bit_start[parent], len, index);
            index+=len;
        }

        len = subtree_bit_end[parent] - subtree_bit_end[completed] - 1;
        if(len>0)
        {
            dfs_structure |= STRUCTURE_BITSELECT(structure, subtree_bit_end[completed]+1, len, index);
            index+=len;
        }
    }

    assert(number_of_vertices(dfs_structure)==number_of_vertices());


    //Now perform a dfs visit and reconstruct the treelet
    Treelet::treelet_structure_t subtrees[16];
    unsigned int nsubtrees=0;

    unsigned int dfs_parents[16] = {0};
    unsigned int num_children[16] = {0};
    unsigned int current = 0;
    unsigned int num_vertices = 1;

    while(true)
    {
        if(dfs_structure & treelet_structure_highest_bit) //new edge
        {
            num_children[current]++;
            dfs_parents[num_vertices] = current;
            current = num_vertices++;
        }
        else
        {
            nsubtrees -= num_children[current];
            std::sort(subtrees + nsubtrees, subtrees + nsubtrees + num_children[current], [] (const Treelet::treelet_structure_t x, const Treelet::treelet_structure_t y) { return x > y; } );

            Treelet::treelet_structure_t s = singleton_structure;
            int idx = 0;
            for(unsigned int i = 0; i < num_children[current]; i++)
            {
                s |= (treelet_structure_highest_bit>>idx);
                s |= (subtrees[nsubtrees + i] >> (idx+1));
                idx += 2 + 2*popcount32(subtrees[nsubtrees + i]);
            }

            subtrees[nsubtrees++] = s;


            if(current==0)
                break;

            current = dfs_parents[current];
        }

        dfs_structure <<= 1;
    }

    assert(nsubtrees==1);
    assert(number_of_vertices(subtrees[0]) == number_of_vertices());
    return Treelet(subtrees[0], colors);
}
