//
// Created by steven on 11/27/16.
//

#include "TreeletSampler.h"
#include "ReservoirSampler.h"

bool TreeletSampler::sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence)
{
    assert(t.is_valid());

    *occurrence = u;

    if(t.number_of_vertices() == 1)
        return true;

    ReservoirSampler<std::pair<Treelet, UndirectedGraph::vertex_t> > sampler(std::make_pair(Treelet::invalid_treelet, 0), rng);

    Treelet split = t.split_child();
    assert(!split.is_colored());

    const TreeletTable *table = table_collection->get_table(split.number_of_vertices());

    const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
    for(UndirectedGraph::vertex_t d = 0; d < graph->degree(u); ++d)
    {
        const UndirectedGraph::vertex_t v = neighbors[d];
        for(TreeletTable::const_iterator it = table->begin(v, split); it != table->end(v); ++it)
        {
            const Treelet& t2 = it.treelet();
            if(t2.get_structure() != split.get_structure())
                break;

            if((t2.get_colors() & ~t.get_colors()) == 0 )
            {
                Treelet complement = t.complement(t2);
                TreeletTable::treelet_count_t c = table_collection->get_table(complement.number_of_vertices())->get_count(u, complement);

                assert(it.count()!=0);
                sampler.feed(std::make_pair(t2, v), c * it.count());
            }
        }
    }

    assert(sampler.get_total_weight()!=0);
    assert(table_collection->get_table(t.number_of_vertices())->get_count(u, t) * t.normalization_factor() == sampler.get_total_weight());

    auto child = sampler.get_sample();
    if(!child.first.is_valid())
        return false;

    Treelet complement = t.complement(child.first);

    return (complement.is_singleton() || sample_rooted_occurrence(complement, u, occurrence + child.first.number_of_vertices()) ) &&
            sample_rooted_occurrence(child.first, child.second, occurrence+1);
}

