//
// Created by steven on 11/13/16.
//

#include <istream>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <sys/mman.h>
#include "UndirectedGraph.h"

UndirectedGraph::UndirectedGraph(const std::string &basename)
{
    offsets_fd = fopen( (basename+".gof").c_str(), "rb" );
    edges_fd = fopen( (basename+".ged").c_str(), "rb" );

    if(offsets_fd==NULL || edges_fd==NULL)
        throw std::runtime_error("Could not open files");


    fread(&num_verts, sizeof(vertex_t), 1, offsets_fd);
    fread(&num_edges, sizeof(uint32_t), 1, offsets_fd);
    offsets = static_cast<vertex_t*>(mmap(nullptr, (num_verts+2)*sizeof(vertex_t), PROT_READ, MAP_PRIVATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 2;

    edges = static_cast<vertex_t*>(mmap(nullptr, (num_edges)*sizeof(uint32_t), PROT_READ, MAP_PRIVATE, fileno(edges_fd), 0));
    assert(edges!=MAP_FAILED);
}

UndirectedGraph::~UndirectedGraph()
{
    munmap(offsets-2, (num_verts+2)*sizeof(vertex_t));
    fclose(offsets_fd);

    munmap(edges, (num_edges)*sizeof(uint32_t));
    fclose(edges_fd);
}

bool UndirectedGraph::has_edge(const vertex_t u, const vertex_t v) const
{
    assert(u<num_verts);
    assert(v<num_verts);

    if(offsets[u+1]-offsets[u]>=offsets[v+1]-offsets[v])
        return std::binary_search(edges+offsets[u], edges+offsets[u+1], v);
    else
        return std::binary_search(edges+offsets[v], edges+offsets[v+1], u);
}

