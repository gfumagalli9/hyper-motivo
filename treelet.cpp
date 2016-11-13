//
// Created by steven on 11/13/16.
//

#include <cstdint>
#include "treelet.h"

using namespace Treelet;

uint64_t Treelet::singleton(int color)
{
    treelet_t_union t;
    t.fields.colors = (uint16_t(1) << color);
    t.fields.structure = 0b10;
    t.fields.num_isomorphic_to_largest = 0;
    t.fields.size = 1;

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

treelet_t Treelet::merge(treelet_t t1, treelet_t t2)
{
    treelet_t_union& t1p = *reinterpret_cast< treelet_t_union* >(&t1);
    treelet_t_union& t2p = *reinterpret_cast< treelet_t_union* >(&t2);

    if(t1p.fields.colors & t2p.fields.colors) //colors intersect
        return invalid_treelet;

    treelet_t last_child_str = (t1p.fields.size>1)?last_child_structure(t1p.fields.structure):invalid_treelet;
    if(last_child_str > t2p.fields.structure ) //explot the fact that invalid_treelet < any_valid_treelet
        return invalid_treelet;

    treelet_t_union t;
    t.fields.colors = t1p.fields.colors | t2p.fields.colors;
    t.fields.structure = (t1p.fields.structure << (2*t2p.fields.size)) + (t2p.fields.structure << 1);
    t.fields.size = t1p.fields.size + t2p.fields.size;
    t.fields.num_isomorphic_to_largest = (uint8_t)((last_child_str== t2p.fields.structure)?(t1p.fields.num_isomorphic_to_largest + 1):1);

    return t.int_repr;
}

uint8_t Treelet::normalization_factor(treelet_t t)
{
    return reinterpret_cast< treelet_t_union* >(&t)->fields.num_isomorphic_to_largest;
}