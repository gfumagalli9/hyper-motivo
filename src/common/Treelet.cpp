//
// Created by steven on 11/13/16.
//

#include <cstdint>
#include <climits>
#include "Treelet.h"

const Treelet Treelet::invalid_treelet = Treelet(invalid_structure, 0); //A generic invalid treelet representation
const Treelet Treelet::invalid_merge_colors = Treelet(invalid_structure, 1); //Merge failed due to intersecting colors
const Treelet Treelet::invalid_merge_structure = Treelet(invalid_structure, 2); //Merge failed due to wrong structure order



/*
uint32_t last_child_structure(uint32_t structure)
{
    uint32_t structure_bit = 1;
    uint32_t last_child_mask = 0;
    int depth = 0;

    do
    {
        last_child_mask |= structure_bit;
        depth += (structure & structure_bit) ? -1 : 1;
        structure_bit <<= 1;
    } while(depth > 0);


    return (structure & last_child_mask) >> 1;
}*/

Treelet Treelet::merge(const Treelet other) const
{
    if(colors & other.colors) //colors intersect
        return invalid_merge_colors;

    const unsigned int other_size = other.number_of_vertices();
    treelet_structure_t new_structure = treelet_structure_highest_bit + (other.structure >> 1) + (structure >> (2*other_size));

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
/*
Treelet Treelet::split_child() const
{
    treelet_structure_t father_structure = structure;
    treelet_structure_t child_structure = 0;

    int depth = 0;
    int child_size=0;
    do
    {
        child_size++;
        child_structure<<=1;

        if(father_structure & treelet_structure_highest_bit)
        {
            depth++;
            child_structure |= 1;
        }
        else
            depth--;

        father_structure<<=1;
    } while(depth);

    child_structure<<=(treelet_structure_bits-child_size+1);

    assert(child_size>0);
    assert(child_size%2==0);
    child_size/=2;

    return Treelet(child_structure, child_size);

}*/

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


