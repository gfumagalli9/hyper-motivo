//
// Created by steven on 11/13/16.
//

#include <istream>
#include <fstream>
#include <algorithm>
#include <sys/mman.h>
#include "UndirectedGraph.h"
#include "../platform/platform.h"

UndirectedGraph::UndirectedGraph(const std::string &basename)
{
    std::string offsets_filename = basename+".gof";
    offsets_fd = fopen( offsets_filename.c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file "+ offsets_filename);

    std::string edges_filename = basename+".ged";
    edges_fd = fopen( edges_filename.c_str(), "rb" );
    if(edges_fd==NULL)
        throw std::runtime_error("Could not open file "+ edges_filename);

    fread(&num_verts, sizeof(vertex_t), 1, offsets_fd);
    fread(&num_edges, sizeof(uint32_t), 1, offsets_fd);
    offsets = static_cast<vertex_t*>(motivo_mmap((num_verts+2)*sizeof(vertex_t), PROT_READ, fileno(offsets_fd)));
    assert(offsets!=MAP_FAILED);
    offsets += 2;

    edges = static_cast<vertex_t*>(motivo_mmap(2*num_edges*sizeof(uint32_t), PROT_READ, fileno(edges_fd)));
    assert(edges!=MAP_FAILED);
}

UndirectedGraph::~UndirectedGraph()
{
    motivo_munmap(offsets-2, (num_verts+2)*sizeof(vertex_t));
    fclose(offsets_fd);

    motivo_munmap(edges, 2*num_edges*sizeof(uint32_t));
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

