//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_UNDIRECTEDGRAPH_H
#define MOTIVO_UNDIRECTEDGRAPH_H

#include <string>
#include <cassert>

///Represents an immutable undirected unweighted graph
///Vertices are numbered with consecutive integers, starting from 0
class UndirectedGraph
{
public:
    typedef uint32_t vertex_t;

private:
    vertex_t num_verts;
    uint32_t num_edges;

    FILE* offsets_fd;
    FILE* edges_fd;

    uint32_t* offsets;
    vertex_t* edges;


public:
    UndirectedGraph(const std::string& filename);
    ~UndirectedGraph();

    ///@returns the number of vertices of the graph
    vertex_t number_of_vertices() const { return num_verts; };

    ///@returns the number of edges of the graph
    uint32_t number_of_edges() const { return num_edges; };

    ///@returns the degree of vertex @param v
    vertex_t degree(const vertex_t v) const { assert(v<num_verts); return offsets[v+1] - offsets[v]; };

    ///@returns an array of degree[v] elements containing the neighbors of vertex @param v. The array must not be freed.
    const vertex_t* neighbors(const vertex_t v) const { assert(v<num_verts); return edges + offsets[v]; }

    ///@returns true iff there is an edge between vertex @param u and vertex @param v
    bool has_edge(const vertex_t u, const vertex_t v) const;
};


#endif //MOTIVO_UNDIRECTEDGRAPH_H
