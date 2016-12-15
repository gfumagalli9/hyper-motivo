//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include "../sampler/ReservoirSampler.h"
#include <stdexcept>
#include <sys/mman.h>
#include <algorithm>

TreeletTable::TreeletTable(const std::string& basename)
{
    std::string offsets_filename = basename+".off";
    offsets_fd = fopen( offsets_filename.c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file " + offsets_filename);

    fread(&num_vertices, sizeof(uint64_t), 1, offsets_fd);
    offsets = static_cast<uint64_t*>(mmap(nullptr, (num_vertices+2)*sizeof(uint64_t), PROT_READ, MAP_PRIVATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 1;

    assert(offsets[num_vertices] != 0); //FIXME: Handle empty table

    std::string data_filename = basename+".dat";
    data_fd = fopen( data_filename.c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file " + data_filename);

    data = static_cast<treelet_count_pair*>(mmap(nullptr, offsets[num_vertices] * sizeof(treelet_count_pair), PROT_READ, MAP_PRIVATE, fileno(data_fd), 0));
    assert(data!=MAP_FAILED);

    try
    {
        root_sampler = new AliasMethodSampler(basename+".rts");
    }
    catch(...)
    {
        root_sampler = nullptr;
    }
}

TreeletTable::~TreeletTable()
{
    munmap(data-1, offsets[num_vertices] * sizeof(treelet_count_pair));
    fclose(data_fd);

    munmap(offsets-1, (num_vertices+2)*sizeof(uint64_t));
    fclose(offsets_fd);

    if(root_sampler)
        delete root_sampler;
}

///@returns a pointer to the first treelet_count_pair in the range [begin, end) whose treelet is greater than or equal to "treelet"
///If now such treelet_count_pair exists, returns @param end
static const TreeletTable::treelet_count_pair* treelet_upper_bound(const TreeletTable::treelet_count_pair *begin,
       const TreeletTable::treelet_count_pair *end, const Treelet &treelet)
{
    while(begin<end-1)
    {
        const TreeletTable::treelet_count_pair *mid = begin + (end - begin) / 2;
        if(mid->treelet <= treelet)
            begin=mid;
        else
            end=mid;
    }
    return begin;
}


///@returns a pointer to the first treelet_count_pair in the range [begin, end) whose count is greater than or equal to "count"
///If now such treelet_count_pair exists, returns @param end
static const TreeletTable::treelet_count_pair* count_upper_bound(const TreeletTable::treelet_count_pair *begin,
        const TreeletTable::treelet_count_pair *end, TreeletTable::treelet_count_t count)
{
    while(begin<end-1)
    {
        const TreeletTable::treelet_count_pair *mid = begin + (end - begin) / 2;
        if(mid->count <= count)
            begin=mid;
        else
            end=mid;
    }
    return begin;
}

TreeletTable::treelet_count_t TreeletTable::get_count(const UndirectedGraph::vertex_t u, const Treelet treelet) const
{
    const treelet_count_pair *tcp = treelet_upper_bound(data + offsets[u] + 1, data + offsets[u + 1], treelet);
    if(tcp!=data+offsets[u+1] && tcp->treelet==treelet)
        return tcp->count;

    return 0;
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u) const
{
    assert(u<num_vertices);
    return TreeletTable::const_iterator( data + offsets[u] + 1 );
}

TreeletTable::const_iterator TreeletTable::end(const UndirectedGraph::vertex_t u) const
{
    assert(u<num_vertices);
    return TreeletTable::const_iterator( data + offsets[u+1] );
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u, Treelet treelet) const
{
    assert(u<num_vertices);
    const treelet_count_pair* tcp = treelet_upper_bound(data + offsets[u] + 1, data + offsets[u + 1], treelet);
    return TreeletTable::const_iterator(tcp);
}

UndirectedGraph::vertex_t TreeletTable::get_random_root(Random *rng) const
{
    if(root_sampler)
        return static_cast<UndirectedGraph::vertex_t>(root_sampler->sample(rng));

    ReservoirSampler<UndirectedGraph::vertex_t> sampler(0, rng);
    for(UndirectedGraph::vertex_t u=0; u<num_vertices; u++)
    {
        treelet_count_t ntreelets = (data + offsets[u+1]-1)->count;
        sampler.feed(u, ntreelets);
    }

    return sampler.get_sample();
}

const Treelet& TreeletTable::get_random_treelet(UndirectedGraph::vertex_t root, Random* rng) const
{
    assert(root<num_vertices);

    treelet_count_t ntreelets =  (data + offsets[root+1]-1)->count; //Number of treelets rooted in root

    if(ntreelets==0)
        return Treelet::invalid_treelet;

    treelet_count_t r =  rng->random_uint64(1, ntreelets+1);
    const treelet_count_pair *tcp = count_upper_bound(data + offsets[root]+1, data + offsets[root+1], r);
    assert(tcp!=data + offsets[root+1]);
    assert(tcp->treelet.is_valid());
    return tcp->treelet;
}
