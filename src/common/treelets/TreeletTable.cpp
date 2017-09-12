//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include "../../platform/platform.h"

TreeletTable::TreeletTable(BaseRecordSource<const treelet_count_pair_maybe_alias>* record_sorce) : reader(record_sorce)
{
    num_vertices = static_cast<UndirectedGraph::vertex_t>(reader->number_of_records());
    assert(num_vertices < std::numeric_limits<UndirectedGraph::vertex_t>::max()-1);
    root_sampler = nullptr;
}

void TreeletTable::load_root_sampler(const std::string& filename)
{
    if(!root_sampler)
        root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, treelet_count_t>(filename);
}

TreeletTable::~TreeletTable()
{
    if(root_sampler)
        delete root_sampler;
}

///@returns a pointer to the first treelet_count_pair in the range [begin, end) whose treelet is greater than or equal to "treelet"
///If no such treelet_count_pair exists, returns @param end
const TreeletTable::treelet_count_pair_maybe_alias* TreeletTable::treelet_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin,
       const TreeletTable::treelet_count_pair_maybe_alias *end, const Treelet &treelet)
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
        const TreeletTable::treelet_count_pair_maybe_alias *mid = begin + (end - begin) / 2;
        if(mid->treelet < treelet)
            begin=mid+1;
        else
            end=mid;
    }

    return begin;
}


///@returns a pointer to the first treelet_count_pair in the range [begin, end) whose count is greater than or equal to "count"
///If no such treelet_count_pair exists, returns @param end
const TreeletTable::treelet_count_pair_maybe_alias* TreeletTable::count_upper_bound(const TreeletTable::treelet_count_pair_maybe_alias *begin,
        const TreeletTable::treelet_count_pair_maybe_alias *end, TreeletTable::treelet_count_t count)
{
    while(begin<end)
    {
        const TreeletTable::treelet_count_pair_maybe_alias *mid = begin + (end - begin) / 2;
        if(mid->count < count)
            begin=mid+1;
        else
            end=mid;
    }
    return begin;
}

TreeletTable::treelet_count_t TreeletTable::get_count(const UndirectedGraph::vertex_t u, const Treelet treelet) const
{
    assert(u<num_vertices);
    Record<const treelet_count_pair_maybe_alias> record = reader->get_record(u);

    const treelet_count_pair_maybe_alias *tcp = treelet_upper_bound(record.begin()+1, record.end(), treelet);
    TreeletTable::treelet_count_t count = 0;
    if(tcp!= record.end() && tcp->treelet==treelet)
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
    Record<const treelet_count_pair_maybe_alias> record = reader->get_record(u);

    return TreeletTable::const_iterator( record, treelet_upper_bound(record.begin()+1, record.end(), treelet));
}

const Treelet TreeletTable::get_random_treelet(UndirectedGraph::vertex_t root, Random* rng)
{
    assert(root<num_vertices);
    Record<const treelet_count_pair_maybe_alias> record = reader->get_record(root);

    if(record.length()==0)
    {
        record.free();
        return Treelet::invalid_treelet;
    }

    assert((record.end()-1)->count!=0);

    treelet_count_t r =  rng->random_uint<treelet_count_t>(1, (record.end()-1)->count);
    const treelet_count_pair_maybe_alias *tcp = count_upper_bound(record.begin()+1, record.end(), r);
    assert(tcp!=record.end());
    assert(tcp->treelet.is_valid());
    record.free();
    return tcp->treelet;
}
