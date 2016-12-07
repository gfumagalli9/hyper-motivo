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
    offsets_fd = fopen( (basename+".off").c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file");

    fread(&num_vertices, sizeof(uint64_t), 1, offsets_fd);
    offsets = static_cast<uint64_t*>(mmap(NULL, (num_vertices+1)*sizeof(uint64_t), PROT_READ, MAP_PRIVATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 1;

    assert(offsets[num_vertices] != 0); //FIXME: Handle empty table
    data_fd = fopen( (basename+".dat").c_str(), "rb" );
    data = static_cast<treelet_count_pair*>(mmap(nullptr, (offsets[num_vertices]+1) * sizeof(treelet_count_pair), PROT_READ, MAP_PRIVATE, fileno(data_fd), 0));
    assert(data!=MAP_FAILED);
    data += 1;

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
    munmap(data-1, (offsets[num_vertices]+1) * sizeof(treelet_count_pair));
    fclose(data_fd);

    munmap(offsets-1, num_vertices*sizeof(uint64_t));
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
    const treelet_count_pair *tcp = treelet_upper_bound(data + offsets[u], data + offsets[u + 1], treelet);
    if(tcp!=data+offsets[u+1] && tcp->treelet==treelet)
        return tcp->count;

    return 0;
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u) const
{
    assert(u<num_vertices);
    return TreeletTable::const_iterator( data + offsets[u] );
}

TreeletTable::const_iterator TreeletTable::end(const UndirectedGraph::vertex_t u) const
{
    assert(u<num_vertices);
    return TreeletTable::const_iterator( data + offsets[u+1] );
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u, Treelet treelet) const
{
    assert(u<num_vertices);
    const treelet_count_pair* tcp = treelet_upper_bound(data + offsets[u], data + offsets[u + 1], treelet);
    return TreeletTable::const_iterator(tcp);
}

UndirectedGraph::vertex_t TreeletTable::get_random_root(Random *rng) const
{
    if(root_sampler)
        return static_cast<UndirectedGraph::vertex_t>(root_sampler->sample(rng));

    ReservoirSampler<UndirectedGraph::vertex_t> sampler(0, rng);
    treelet_count_t before=0;
    for(UndirectedGraph::vertex_t u; u<num_vertices; u++)
    {
        treelet_count_t after = (data + offsets[u+1]-1)->count;
        sampler.feed(u, after-before);
        before=after;
    }

    return sampler.get_sample();
}

const Treelet& TreeletTable::get_random_treelet(UndirectedGraph::vertex_t root, Random* rng) const
{
    assert(root<num_vertices);

    treelet_count_t before = (data+offsets[root]-1)->count; //Number of treelets before root
    treelet_count_t after =  (data + offsets[root+1]-1)->count; //Number of treelets before after root.

    if(before==after) //There are no treelets rooted in root
        return Treelet::invalid_treelet;

    treelet_count_t r = before + rng->random_uint64(1, after-before+1);
    const treelet_count_pair *tcp = count_upper_bound(data + offsets[root], data + offsets[root+1], r);
    assert(tcp!=data + offsets[root+1]);
    assert(tcp->treelet.is_valid());
    return tcp->treelet;
}
