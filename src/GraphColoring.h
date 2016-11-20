//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_GRAPHCOLORING_H
#define MOTIVO_GRAPHCOLORING_H


#include <cstdint>

class GraphColoring
{
    typedef uint8_t color_t;

private:
    color_t* colors;

public:
    ///Contructs a random coloring of @param n vertices using @param numColors colors
    GraphColoring(long n, int numColors);

    ///@returns the color of vertex @param u
    color_t color_of(long u) const { return colors[u]; };
};


#endif //MOTIVO_GRAPHCOLORING_H
