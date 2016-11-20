//
// Created by steven on 11/13/16.
//

#include <fstream>
#include <stdexcept>
#include "Graph.h"

Graph::Graph(const std::string &filename)
{
    std::ifstream file(filename);
    if(!file.is_open())
        throw new std::runtime_error("Unable to open file");

    file >> num_verts >> num_edges;

    degrees = new long[num_verts];
    adjLists = new long*[num_verts];

    for(long u=0; u<num_verts; u++)
    {
        file >> degrees[u];
        adjLists[u] = new long[degrees[u]];

        for(int i=0; i<degrees[u]; i++)
            file >> adjLists[u][i];
    }
}

Graph::~Graph()
{
    for(long u=0; u<num_verts; u++)
        delete[] adjLists[u];

    delete[] adjLists;
    delete[] degrees;
}
