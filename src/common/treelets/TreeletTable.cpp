// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "TreeletTable.h"
#include "TreeletStructureSelector.h"
#include "../platform/platform.h"
#include "../random/RangeSampler.h"

TreeletTable::TreeletTable(BaseRecordSource<const treelet_count_pair_maybe_alias>* record_source) : reader(record_source)
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
        return invalid_treelet;
    }

    assert((record.end()-1)->count!=0);

    treelet_count_t r =  rng->random_uint<treelet_count_t>(1, (record.end()-1)->count);
    const treelet_count_pair_maybe_alias *tcp = count_upper_bound(record.begin()+1, record.end(), r);
    assert(tcp!=record.end());
    assert(tcp->treelet.is_valid());
    Treelet t = tcp->treelet;
    record.free();
    return t;
}

const Treelet TreeletTable::get_treelet_no(UndirectedGraph::vertex_t root, TreeletTable::treelet_count_t no) const
{
    assert(root<num_vertices);
    Record<const treelet_count_pair_maybe_alias> record = reader->get_record(root);

    if(record.length()==0)
    {
        record.free();
        return invalid_treelet;
    }

    const treelet_count_pair_maybe_alias *tcp = count_upper_bound(record.begin()+1, record.end(), no+1);
    assert(tcp!=record.end());
    assert(tcp->treelet.is_valid());
    Treelet t = tcp->treelet;
    record.free();
    return t;
}


RangeSampler<TreeletTable::treelet_count_t>* TreeletTable::build_range_sampler(const UndirectedGraph::vertex_t u, const TreeletStructureSelector* selector)
{
    Record<const treelet_count_pair_maybe_alias> record = reader->get_record(u);

    const treelet_count_pair_maybe_alias* next_to_add = record.begin()+1;
    auto* rs = new RangeSampler<TreeletTable::treelet_count_t>();

    if(selector)
    {
        assert(selector->size()<2 || *selector->begin() > *(++selector->begin()) );

        for(Treelet::treelet_structure_t structure : *selector)
        {
            //FIXME: start binary search from last added treelet
            //first treelet of interest (inclusive). If none, next treelet
            const treelet_count_pair_maybe_alias *tcp_lower = treelet_upper_bound(record.begin() + 1, record.end(), Treelet(structure));
            const treelet_count_pair_maybe_alias *tcp_upper; //one after the last treelet of interest

            Treelet last(structure, Treelet::all_colors);
            tcp_upper = treelet_upper_bound(tcp_lower, record.end(), last);
            if(tcp_upper!=record.end() && tcp_upper->treelet==last)
                tcp_upper++;

            if(tcp_lower==tcp_upper)
                continue;

            if(selector->get_mode()==TreeletStructureSelector::MODE_INCLUDE)
                rs->add_range((tcp_lower - 1)->count, (tcp_upper - 1)->count);
            else
            {
                if(next_to_add!=tcp_lower)
                    rs->add_range((next_to_add - 1)->count, (tcp_lower - 1)->count);

                next_to_add = tcp_upper;
            }
        }
    }

    if( (!selector || selector->get_mode()==TreeletStructureSelector::MODE_EXCLUDE) && next_to_add!=record.end() )
        rs->add_range((next_to_add - 1)->count, (record.end() - 1)->count);


    record.free();

    return rs;
}
