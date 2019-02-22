//
// Created by steven on 11/27/16.
//

#include <thread>
#include "TreeletSampler.h"

TreeletSampler::TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, const unsigned int size)
        : graph(graph), table_collection(ttc), size(size)
{
}

void TreeletSampler::set_selector(const TreeletStructureSelector *selector, unsigned int nthreads)
{
    if(this->selector)
    {
        for(UndirectedGraph::vertex_t u=0; u<graph->number_of_vertices(); u++)
            delete[] range_samplers[u];

        delete range_samplers;
        delete root_sampler;
        range_samplers = nullptr;
        root_sampler = nullptr;
    }

    this->selector = selector;
    
    if(selector == nullptr)
        return;

    range_samplers = new RangeSampler<TreeletTable::treelet_count_t>*[graph->number_of_vertices()];
    root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t,TreeletTable::treelet_count_t>(graph->number_of_vertices());

    if(nthreads<=1)
    {
        for (UndirectedGraph::vertex_t u = 0; u < graph->number_of_vertices(); u++)
        {
            range_samplers[u] = table_collection->get_table(size)->build_range_sampler(u, selector);
            root_sampler->set(u, range_samplers[u]->get_total_length());
        }
    }
    else
    {
        auto worker_threads = new std::thread[nthreads];
        DynamicSequencer<UndirectedGraph::vertex_t>  sequencer(0, graph->number_of_vertices(), nthreads);
        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i] = std::thread( [this, &sequencer] {populate_root_and_range_sampler_mt(sequencer);});

        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
    }

    for (UndirectedGraph::vertex_t u = 0; u < graph->number_of_vertices(); u++)
        root_sampler->set(u, range_samplers[u]->get_total_length());

    root_sampler->build();

}

void TreeletSampler::populate_root_and_range_sampler_mt(DynamicSequencer<UndirectedGraph::vertex_t> &sequencer)
{
    while(true)
    {
        DynamicSequencer<UndirectedGraph::vertex_t>::sequence_batch_t batch = sequencer.next_batch();
        if (batch.from >= batch.to_exclusive)
            break;

        for (UndirectedGraph::vertex_t u = batch.from; u < batch.to_exclusive; u++)
            range_samplers[u] = table_collection->get_table(size)->build_range_sampler(u, selector);
    }
}



TreeletSampler::~TreeletSampler()
{
    if(selector)
    {
        delete root_sampler;
        for(UndirectedGraph::vertex_t u=0; u<graph->number_of_vertices(); u++)
            delete range_samplers[u];

        delete range_samplers;
    }
}


bool TreeletSampler::sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence, Random *rng)
{
    assert(t.is_valid());

    *occurrence = u;

    if(t.number_of_vertices() == 1)
        return true;

    Treelet split = t.split_child();
    assert(!split.is_colored());

    TreeletTable *table = table_collection->get_table(t.number_of_vertices());
    TreeletTable *split_table = table_collection->get_table(split.number_of_vertices());

    TreeletTable::treelet_count_t count = table->get_count(u, t);
    if(count==0)
        return false;

    safe_mul(count, t.normalization_factor(), &count);
    TreeletTable::treelet_count_t r = rng->random_uint<TreeletTable::treelet_count_t>(0,  count-1);

    Treelet child_treelet = invalid_treelet;
    UndirectedGraph::vertex_t child_vertex=0;

    const UndirectedGraph::vertex_t degree = graph->degree(u);
    for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
    {
        const UndirectedGraph::vertex_t v = graph->neighbor(u, d);
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

    return ( complement.is_singleton() || sample_rooted_occurrence(complement, u, occurrence + child_treelet.number_of_vertices(), rng) ) &&
            sample_rooted_occurrence(child_treelet, child_vertex, occurrence+1, rng);
}




