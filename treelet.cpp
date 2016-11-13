//
// Created by steven on 11/13/16.
//

#include <cstdint>
#include "treelet.h"

using namespace Treelet;

uint64_t Treelet::singleton(int color)
{
    treelet_t_packed t;
    t.colors = (uint16_t(1) << color);
    t.structure = 0b10;
    t.num_isomorphic_to_largest = 0;
    t.size = 1;

    return t.int_repr;
}

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

}

bool Treelet::is_mergeable(treelet_t t1, treelet_t t2)
{
    treelet_t_packed& t1p = *reinterpret_cast< treelet_t_packed* >(&t1);
    treelet_t_packed& t2p = *reinterpret_cast< treelet_t_packed* >(&t2);

    if(t1p.colors & t2p.colors) //colors intersect
        return false;

    return !(t1p.size != 0 && last_child_structure(t1p.structure) > t2p.structure);
}

treelet_t Treelet::merge(treelet_t t1, treelet_t t2)
{
    treelet_t_packed& t1p = *reinterpret_cast< treelet_t_packed* >(&t1);
    treelet_t_packed& t2p = *reinterpret_cast< treelet_t_packed* >(&t2);

    treelet_t_packed t;
    t.colors = t1p.colors | t2p.colors;
    t.structure = (t1p.structure << (2*t2p.size)) + (t2p.structure << 1);
    t.size = t1p.size + t2p.size;

    if(t1p.size != 0 && last_child_structure(t1p.structure) == t2p.structure)
        t.num_isomorphic_to_largest = (uint8_t) (t1p.num_isomorphic_to_largest + 1);
    else
        t.num_isomorphic_to_largest = 1;

    return t.int_repr;
}

uint8_t Treelet::normalization_factor(treelet_t t)
{
    return reinterpret_cast< treelet_t_packed* >(&t)->num_isomorphic_to_largest;
}