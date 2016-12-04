//
// Created by steven on 11/13/16.
//

#include "GraphColoring.h"
#include "Random.h"

GraphColoring::GraphColoring(const UndirectedGraph::vertex_t n, unsigned int number_of_colors)
{
    Random r;
    colors = new color_t[n];
    for(UndirectedGraph::vertex_t u=0; u<n; u++)
        colors[u] = (color_t)r.random_uint32(0, number_of_colors);
}
