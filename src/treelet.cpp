//
// Created by steven on 11/13/16.
//

#include <cstdint>
#include "treelet.h"

using namespace Treelet;

uint64_t Treelet::singleton(uint16_t color)
{
    treelet_t_union t;
    t.fields.colors = (uint16_t(1) << color);
    t.fields.structure = 0;
    t.fields.children_isomorphic_to_largest = 0;
    t.fields.size = 1;

    return t.int_repr;
}

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

treelet_t Treelet::merge(treelet_t t1, treelet_t t2)
{
    treelet_t_union& t1p = *reinterpret_cast< treelet_t_union* >(&t1);
    treelet_t_union& t2p = *reinterpret_cast< treelet_t_union* >(&t2);

    if(t1p.fields.colors & t2p.fields.colors) //colors intersect
        return invalid_treelet;

    treelet_t_union t;
    t.fields.structure = 0x80000000 + (t2p.fields.structure >> 1) + (t1p.fields.structure >> (2*t2p.fields.size));

    //Let x be the first child of t1. Let |t| denote the size of t.
    //If x and t2 coincide then there the first 2*max(|x|,|t2|) bits of the structure of t1 and t2 coincide
    //If x and t2 differ then there at least one bit in the first 2*min(|x|,|t2|) bits of the structure of t1 and t2 differs
    if( (t1p.fields.structure ^ t.fields.structure) >> (32 - 2*t2p.fields.size) ) //True iff x and t2 differ.
    {
        if( t1p.fields.structure > t.fields.structure ) //The first differing bit determines the result
            return invalid_treelet;

        t.fields.children_isomorphic_to_largest=1; //t2 itself
    }
    else
        t.fields.children_isomorphic_to_largest= static_cast<uint8_t>(t1p.fields.children_isomorphic_to_largest+1);

    t.fields.size = t1p.fields.size + t2p.fields.size;
    t.fields.colors = t1p.fields.colors | t2p.fields.colors;

    return t.int_repr;
}

uint8_t Treelet::normalization_factor(treelet_t t)
{
    return reinterpret_cast< treelet_t_union* >(&t)->fields.children_isomorphic_to_largest;
}

treelet_structure_t Treelet::structure(treelet_t t)
{
    return  reinterpret_cast< treelet_t_union* >(&t)->fields.structure;
}