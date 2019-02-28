//
// Created by steven on 12/22/18.
//

#ifndef MOTIVO_SIZE1BUILDER_H
#define MOTIVO_SIZE1BUILDER_H

#include <ostream>
#include "../common/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/TreeletTable.h"

//FIXME: Should the TreeletSelector also apply to size 1 tables?

class Size1Builder
{

private:
    const UndirectedGraph::vertex_t number_of_vertices;
    const UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    const uint8_t number_of_colors;
    const bool store_only_0;
    const double bias;
    Random* const rng;
    std::ostream* const output;
    double* color_distribution;

public:
    Size1Builder(UndirectedGraph::vertex_t number_of_vertices,UndirectedGraph::vertex_t from_vertex,
                     UndirectedGraph::vertex_t to_vertex,uint8_t number_of_colors, bool store_only_0,
                     double bias, double *color_distribution, Random *rng, std::ostream* output);

    void build();

};

#endif //MOTIVO_SIZE1BUILDER_H
