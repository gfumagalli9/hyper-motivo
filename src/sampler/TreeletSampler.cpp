//
// Created by steven on 11/27/16.
//

#include "TreeletSampler.h"

bool TreeletSampler::sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence)
{
    assert(t.is_valid());

    *occurrence = u;

    if(t.number_of_vertices() == 1)
        return true;

    //ReservoirSampler<std::pair<Treelet, UndirectedGraph::vertex_t> > sampler(std::make_pair(Treelet::invalid_treelet, 0), rng);

    Treelet split = t.split_child();
    assert(!split.is_colored());

    TreeletTable *table = table_collection->get_table(t.number_of_vertices());
    TreeletTable *split_table = table_collection->get_table(split.number_of_vertices());

    TreeletTable::treelet_count_t count = table->get_count(u, t);
    if(count==0)
        return false;

    safe_mul(count, t.normalization_factor(), &count);
    TreeletTable::treelet_count_t r = rng->random_uint<TreeletTable::treelet_count_t>(0,  count-1);

    Treelet child_treelet = Treelet::invalid_treelet;
    UndirectedGraph::vertex_t child_vertex=0;

    const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
    for(UndirectedGraph::vertex_t d = 0; d < graph->degree(u); ++d)
    {
        const UndirectedGraph::vertex_t v = neighbors[d];
        for(TreeletTable::const_iterator it = split_table->begin(v, split); !it.is_over(); ++it)
        {
            const Treelet& t2 = it.treelet();
            if(t2.get_structure() != split.get_structure())
                break;

            if( t2.get_colors() & ~t.get_colors() )
                continue;

            Treelet complement = t.complement(t2);
            TreeletTable::treelet_count_t c = table_collection->get_table(complement.number_of_vertices())->get_count(u, complement);

            assert(it.count()!=0);
            assert(c*it.count() <= count);

#ifndef NDEBUG
            count -= c*it.count();

            if(!child_treelet.is_valid())
#endif
            {
                if (r >= c * it.count())
                    r -= c*it.count();
                else
                {
                    child_treelet = t2;
                    child_vertex = v;
#ifdef NDEBUG
                    goto end_loop; //Children found, exit early
#endif
                }
            }
        }
    }

#ifdef NDEBUG
    end_loop:
#endif

    assert(count == 0);
    assert(child_treelet.is_valid());

    Treelet complement = t.complement(child_treelet);

    return (complement.is_singleton() || sample_rooted_occurrence(complement, u, occurrence + child_treelet.number_of_vertices()) ) &&
            sample_rooted_occurrence(child_treelet, child_vertex, occurrence+1);
}

