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
    fread(&num_edges, sizeof(vertex_t), 1, offsets_fd); //FIXME: use own type?
    offsets = static_cast<char*>(motivo_mmap_populate((num_verts + 2) * sizeof(vertex_t), PROT_READ, fileno(offsets_fd)));
    assert(offsets!=MAP_FAILED);
    offsets += 2*sizeof(vertex_t);

    edges = static_cast<char*>(motivo_mmap_populate(2 * num_edges * sizeof(vertex_t), PROT_READ, fileno(edges_fd)));
    assert(edges!=MAP_FAILED);
}

UndirectedGraph::~UndirectedGraph()
{
    motivo_munmap(offsets-2*sizeof(vertex_t), (num_verts+2)*sizeof(vertex_t));
    fclose(offsets_fd);

    motivo_munmap(edges, 2*num_edges*sizeof(vertex_t));
    fclose(edges_fd);
}

static bool binary_search(const char *begin, const char *end, const UndirectedGraph::vertex_t to_find)
{
    UndirectedGraph::vertex_t t;
    while(begin<end)
    {
        const char* mid = begin + static_cast<UndirectedGraph::vertex_t>(static_cast<uintptr_t>(end-begin)/(2*sizeof(UndirectedGraph::vertex_t)))*sizeof(UndirectedGraph::vertex_t);
        memcpy(&t, mid, sizeof(UndirectedGraph::vertex_t));

        if(t==to_find)
            return true;

        if(to_find<t)
            end=mid;
        else
            begin=mid+sizeof(UndirectedGraph::vertex_t);
    }

    return false;
}


bool UndirectedGraph::has_edge(const vertex_t u, const vertex_t v) const
{
    assert(u<num_verts);
    assert(v<num_verts);

    const char* begin_u = offset_of(u);
    const char* end_u = offset_of(u+1);
    const char* begin_v = offset_of(v);
    const char* end_v = offset_of(v+1);

     if(end_u-begin_u <= end_v-begin_v)
        return binary_search(begin_u, end_u, v);
    else
        return binary_search(begin_v, end_v, u);
}
