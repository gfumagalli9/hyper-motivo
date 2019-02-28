//
// Created by steven on 11/27/16.
//

#include <thread>
#include <map>
#include <queue>
#include "TreeletSampler.h"
#include "../common/sequencer/BaseSequencer.h"
#include "../common/sequencer/DynamicSequencer.h"

TreeletSampler::TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, const unsigned int size)
        : graph(graph), table_collection(ttc), size(size)
{}

void TreeletSampler::set_selector(const TreeletSelector *selector, unsigned int nthreads)
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
        std::thread *worker_threads = new std::thread[nthreads];
        auto* sequencer = new DynamicSequencer<UndirectedGraph::vertex_t>(0, graph->number_of_vertices()-1, nthreads);
        for (unsigned int i = 0; i < nthreads; i++)
        {
            worker_threads[i] = std::thread( [this, sequencer] {populate_root_and_range_sampler_mt(sequencer);});
        }

        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
        delete sequencer;

    }

    for (UndirectedGraph::vertex_t u = 0; u < graph->number_of_vertices(); u++)
        root_sampler->set(u, range_samplers[u]->get_total_length());

    root_sampler->build();

}

void TreeletSampler::populate_root_and_range_sampler_mt(DynamicSequencer<UndirectedGraph::vertex_t>* sequencer)
{
    while(true)
    {
        DynamicSequencer<UndirectedGraph::vertex_t>::sequence_batch_t batch = sequencer->next_batch();
        if (batch.from >= batch.to)
            break;

        for (UndirectedGraph::vertex_t u = batch.from; u < batch.to; u++) {
            range_samplers[u] = table_collection->get_table(size)->build_range_sampler(u, selector);
        }
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

void TreeletSampler::populate_buffer(const UndirectedGraph::vertex_t u, const Treelet& t, Random *rng)
{
    auto &buffer = buffers[ std::make_pair(u, t) ];

    Treelet split = t.split_child();
    assert(!split.is_colored());

    TreeletTable *table = table_collection->get_table(t.number_of_vertices());
    TreeletTable *split_table = table_collection->get_table(split.number_of_vertices());

    TreeletTable::treelet_count_t count = table->get_count(u, t);
    if(count==0)
        return;

    safe_mul(count, t.normalization_factor(), &count);
    TreeletTable::treelet_count_t indices[buffers_size];
    for(TreeletTable::treelet_count_t &r : indices)
        r = rng->random_uint<TreeletTable::treelet_count_t>(0,  count-1);

    std::sort(indices, indices+buffers_size);
    unsigned int found = 0;
    TreeletTable::treelet_count_t seen=0;

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

            seen += c*it.count();

            while(found!=buffers_size && indices[found]<seen)
            {
                found++;
                buffer.push( v, t2 );
            }

            if(found==buffers_size)
                return;
        }
    }

}


bool TreeletSampler::sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence, Random *rng)
{
    assert(t.is_valid());

    *occurrence = u;

    if(t.number_of_vertices() == 1)
        return true;

    const UndirectedGraph::vertex_t degree = graph->degree(u);
    if(degree >= degree_threshold)
    {
        //Try to use cache
        buffers_mutex.lock(); //Accessing the map buffers might cause a new element to get inserted
        auto &buffer = buffers[ std::make_pair(u, t) ];
        buffers_mutex.unlock();

        buffer.lock();
        if(buffer.empty())
            populate_buffer(u, t, rng);

        assert(!buffer.empty());
        auto buffered = buffer.pop(); //FIXME: Use structured bindings in C++17
        auto &child_vertex = buffered.first;
        auto &child_treelet = buffered.second;
        buffer.unlock();

        assert(child_treelet.is_valid());
        assert(child_treelet.is_colored());
        assert(child_treelet.get_structure() == t.split_child().get_structure() );

        Treelet complement = t.complement(child_treelet);
        return ( complement.is_singleton() || sample_rooted_occurrence(complement, u, occurrence + child_treelet.number_of_vertices(), rng) ) && sample_rooted_occurrence(child_treelet, child_vertex, occurrence+1, rng);
    }

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

    return ( complement.is_singleton() || sample_rooted_occurrence(complement, u, occurrence + child_treelet.number_of_vertices(), rng) ) && sample_rooted_occurrence(child_treelet, child_vertex, occurrence+1, rng);
}




