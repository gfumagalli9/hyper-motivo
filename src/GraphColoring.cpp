//
// Created by steven on 11/13/16.
//

#include "GraphColoring.h"
#include "Random.h"

GraphColoring::GraphColoring(long n, int numColors)
{
    Random r;
    colors = new color_t[n];
    for(int u=0; u<n; u++)
        colors[u] = (color_t)r.random_int32(0, numColors);
}
