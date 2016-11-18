//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_GRAPH_H
#define MOTIVO_GRAPH_H

#include <string>
#include <cassert>

///Represents an immutable undirected unweighted graph
///Vertices are numbered with consecutive integers, starting from 0
class Graph
{
private:
    long num_verts;
    long long num_edges;

    long* degrees;
    long** adjLists;

public:
    Graph(const std::string& filename);
    ~Graph();

    ///@returns the number of vertices of the graph
    long number_of_vertices() const { return num_verts; };

    ///@returns the number of edges of the graph
    long number_of_edges() const { return num_edges; };

    ///@returns the degree of vertex v
    long degree(const long v) const { assert(v<num_verts); return degrees[v]; };

    ///@returns the neighbors of vertex v
    const long* neighbors(const long v) const { assert(v<num_verts); return adjLists[v]; }
};


#endif //MOTIVO_GRAPH_H
