//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include "../platform/platform.h"

TreeletTable::TreeletTable(const std::string& basename)
{
    std::string offsets_filename = basename+".off";
    offsets_fd = fopen( offsets_filename.c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file " + offsets_filename);

    uint64_t nverts;
    fread(&nverts, sizeof(uint64_t), 1, offsets_fd);
    assert(nverts < std::numeric_limits<UndirectedGraph::vertex_t>::max()-1);
    num_vertices = static_cast<UndirectedGraph::vertex_t>(nverts);

    offsets = static_cast<uint64_t*>(mmap(nullptr, (num_vertices+2)*sizeof(uint64_t), PROT_READ, MOTIVO_MMAP_FLAGS_PRIVATE_POPULATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 1;

    assert(offsets[num_vertices] != 0); //FIXME: Handle empty table

    std::string data_filename = basename+".dat";
    data_fd = fopen( data_filename.c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file " + data_filename);

    data = static_cast<treelet_count_pair*>(mmap(nullptr, offsets[num_vertices] * sizeof(treelet_count_pair), PROT_READ, MOTIVO_MMAP_FLAGS_PRIVATE_POPULATE, fileno(data_fd), 0));
    //madvise(data, offsets[num_vertices] * sizeof(treelet_count_pair), MADV_SEQUENTIAL);
    assert(data!=MAP_FAILED);

    try
    {
        root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>(basename+".rts");
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
///If no such treelet_count_pair exists, returns @param end
static const TreeletTable::treelet_count_pair* treelet_upper_bound(const TreeletTable::treelet_count_pair *begin,
       const TreeletTable::treelet_count_pair *end, const Treelet &treelet)
{
    //x=first element >= treelet (no duplicate elements) if it exists, otherwise x=end
    //If end-begin>=1.
    //  If x is in [begin, end] at the beginning of an iteration => x is in [begin, end] at the end of the iteration
    //  Proof: mid in [begin, end).
    //  If treelet>mid then x>=treelet>mid and hence x in [mid+1, end] = [begin, end] at the end of the iteration
    //  If treelet<=mid then x<=mid and hence x in [begin, mid] = [begin, end] at the end of iteration
    //If end-begin==0 then x==end==begin

    while(begin<end)
    {
        const TreeletTable::treelet_count_pair *mid = begin + (end - begin) / 2;
        if(mid->treelet < treelet)
            begin=mid+1;
        else
            end=mid;
    }

    return begin;
}


///@returns a pointer to the first treelet_count_pair in the range [begin, end) whose count is greater than or equal to "count"
///If no such treelet_count_pair exists, returns @param end
static const TreeletTable::treelet_count_pair* count_upper_bound(const TreeletTable::treelet_count_pair *begin,
        const TreeletTable::treelet_count_pair *end, TreeletTable::treelet_count_t count)
{
    while(begin<end)
    {
        const TreeletTable::treelet_count_pair *mid = begin + (end - begin) / 2;
        if(mid->count < count)
            begin=mid+1;
        else
            end=mid;
    }
    return begin;
}

TreeletTable::treelet_count_t TreeletTable::get_count(const UndirectedGraph::vertex_t u, const Treelet treelet) const
{
    const treelet_count_pair *tcp = treelet_upper_bound(data + offsets[u] + 1, data + offsets[u+1], treelet);
    if(tcp!=data+offsets[u+1] && tcp->treelet==treelet)
        return tcp->count - (tcp-1)->count;

    return 0;
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u, Treelet treelet) const
{
    assert(u<num_vertices);
    return TreeletTable::const_iterator( treelet_upper_bound(data + offsets[u] + 1, data + offsets[u + 1], treelet) );
}

UndirectedGraph::vertex_t TreeletTable::get_random_root(Random *rng) const
{
    if(root_sampler)
        return root_sampler->sample(rng);

    uint64_t r = rng->random_uint<uint64_t>(0, offsets[num_vertices]-1);

    UndirectedGraph::vertex_t begin = 0;
    UndirectedGraph::vertex_t end = num_vertices + 1;

    ///find the vertex v in the range [begin, end) such that offset[v] is greater than or equal to r
    while(begin<end)
    {
        const UndirectedGraph::vertex_t mid = begin + (end - begin) / 2;
        if(offsets[mid] < r)
            begin=mid+1;
        else
            end=mid;
    }
    return begin-1; //v=begin. Return v-1

}

const Treelet& TreeletTable::get_random_treelet(UndirectedGraph::vertex_t root, Random* rng) const
{
    assert(root<num_vertices);

    treelet_count_t ntreelets =  (data + offsets[root+1]-1)->count; //Number of treelets rooted in root

    if(ntreelets==0)
        return Treelet::invalid_treelet;

    treelet_count_t r =  rng->random_uint<treelet_count_t>(1, ntreelets);
    const treelet_count_pair *tcp = count_upper_bound(data + offsets[root]+1, data + offsets[root+1], r);
    assert(tcp!=data + offsets[root+1]);
    assert(tcp->treelet.is_valid());
    return tcp->treelet;
}
