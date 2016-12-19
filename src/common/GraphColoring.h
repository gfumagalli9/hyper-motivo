//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_GRAPHCOLORING_H
#define MOTIVO_GRAPHCOLORING_H


#include <cstdint>
#include "UndirectedGraph.h"

class GraphColoring
{
    typedef uint8_t color_t;

private:
    color_t* colors;

    GraphColoring(const GraphColoring&) = delete;
    void operator=(const GraphColoring&) = delete;

public:
    ///Contructs a random coloring of @param n vertices using @param number_of_colors colors
    GraphColoring(const UndirectedGraph::vertex_t n, unsigned int number_of_colors);

    ///@returns the color of vertex @param u
    color_t color_of(long u) const { return colors[u]; };
};


#endif //MOTIVO_GRAPHCOLORING_H
