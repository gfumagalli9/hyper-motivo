//
// Created by steven on 11/27/16.
//

#include "TreeletSampler.h"
#include "../ReservoirSampler.h"

bool TreeletSampler::sample_rooted_occurrence(Treelet t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence)
{
    if(t.number_of_vertices() == 1)
    {
        *occurrence = u;
        return true;
    }

    ReservoirSampler<std::pair<Treelet, UndirectedGraph::vertex_t> > sampler(std::make_pair(Treelet::invalid_treelet, 0), &rng);

    Treelet split = t.split_child();
    assert(!split.is_colored());

    const TreeletTable *table = table_collection->get_table(split.number_of_vertices());

    const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
    for(UndirectedGraph::vertex_t d = 0; d < graph->degree(u); ++d)
    {
        const UndirectedGraph::vertex_t v = neighbors[d];
        for(TreeletTable::const_iterator it = table->begin(v, split); it != table->end(v); ++it)
        {
            if(it->treelet.get_structure() != split.get_structure())
                break;

            if((it->treelet.get_colors() & ~t.get_colors()) == 0 )
            {
                Treelet complement = t.complement(it->treelet);
                TreeletTable::treelet_count_t c = table_collection->get_table(complement.number_of_vertices())->get_count(u, complement);

                assert(it->count!=0);

                sampler.feed(std::make_pair(it->treelet, v), c * it->count);
            }
        }
    }

    auto child = sampler.get_sample();
    if(!child.first.is_valid())
        return false;

    Treelet complement = t.complement(child.first);
    return sample_rooted_occurrence(complement, u, occurrence) && sample_rooted_occurrence(child.first, child.second, occurrence+complement.number_of_vertices());

}

void TreeletSampler::sample(const unsigned int size, UndirectedGraph::vertex_t *occurrence)
{
    auto treelet_root_pair = table_collection->get_table(size)->get_random_treelet_root_pair(&rng);
    sample_rooted_occurrence(treelet_root_pair.first, treelet_root_pair.second, occurrence);
}
