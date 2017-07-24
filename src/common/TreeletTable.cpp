//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include "../platform/platform.h"

TreeletTable::TreeletTable(const std::string& basename, const bool load_root_sampler) : reader(basename + ".dtz")
{
    num_vertices = static_cast<UndirectedGraph::vertex_t>(reader.number_of_records());
    assert(num_vertices < std::numeric_limits<UndirectedGraph::vertex_t>::max()-1);

    if(load_root_sampler)
    {
        try
        {
            root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>(basename+".rts");
        }
        catch(...)
        {
            root_sampler = nullptr;
        }
    }
    else
        root_sampler = nullptr;
}

TreeletTable::~TreeletTable()
{
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

TreeletTable::treelet_count_t TreeletTable::get_count(const UndirectedGraph::vertex_t u, const Treelet treelet)
{
    assert(u<num_vertices);
    CompressedRecord record = reader.get_record(u);
    const treelet_count_pair* begin = reinterpret_cast<const treelet_count_pair*>(record.get());
    const treelet_count_pair* end = begin + record.length()/sizeof(treelet_count_pair);


    const treelet_count_pair *tcp = treelet_upper_bound(begin+1, end, treelet);
    TreeletTable::treelet_count_t count = 0;
    if(tcp!= end && tcp->treelet==treelet)
        count = tcp->count - (tcp-1)->count;

    record.free();
    return count;
}

UndirectedGraph::vertex_t TreeletTable::get_random_root(Random *rng) const
{
    if(root_sampler)
        return root_sampler->sample(rng);
    else
        throw std::runtime_error("Root sampler not available");
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u, Treelet treelet)
{
    assert(u<num_vertices);
    CompressedRecord record = reader.get_record(u);
    const treelet_count_pair* begin = reinterpret_cast<const treelet_count_pair*>(record.get());
    const treelet_count_pair* end = begin + record.length()/sizeof(treelet_count_pair);

    return TreeletTable::const_iterator( record, treelet_upper_bound(begin+1, end, treelet), end );
}

const Treelet TreeletTable::get_random_treelet(UndirectedGraph::vertex_t root, Random* rng)
{
    assert(root<num_vertices);
    CompressedRecord record = reader.get_record(root);
    const treelet_count_pair* begin = reinterpret_cast<const treelet_count_pair*>(record.get());
    const treelet_count_pair* end = begin + record.length()/sizeof(treelet_count_pair);

    if(begin==end)
    {
        record.free();
        return Treelet::invalid_treelet;
    }

    assert((end-1)->count!=0);

    treelet_count_t r =  rng->random_uint<treelet_count_t>(1, (end-1)->count);
    const treelet_count_pair *tcp = count_upper_bound(begin+1, end, r);
    assert(tcp!=end);
    assert(tcp->treelet.is_valid());
    record.free();
    return tcp->treelet;
}
