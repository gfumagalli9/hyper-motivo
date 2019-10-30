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

#include <map>
#include <thread>
#include "TreeletSampler.h"

TreeletSampler::TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, const unsigned int size,
        uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree)
        : graph(graph), table_collection(ttc), size(size), buffer_size(buffer_size), buffer_degree(buffer_degree)
{}

void TreeletSampler::set_selector(const TreeletStructureSelector *selector, unsigned int nthreads)
{
    if(use_selector)
    {
        for(UndirectedGraph::vertex_t u=0; u<graph->number_of_vertices(); u++)
            delete range_samplers[u];

        delete[] range_samplers;
        delete root_sampler;
        range_samplers = nullptr;
        root_sampler = nullptr;
    }

    if(selector == nullptr)
    {
        use_selector = false;
        return;
    }

    use_selector = true;

    range_samplers = new RangeSampler<TreeletTable::treelet_count_t>*[graph->number_of_vertices()];
    root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t,TreeletTable::treelet_count_t>(graph->number_of_vertices());

    if(nthreads<=1)
    {
        for (UndirectedGraph::vertex_t u = 0; u < graph->number_of_vertices(); u++)
            range_samplers[u] = table_collection->get_table(size)->build_range_sampler(u, selector);
    }
    else
    {
        auto worker_threads = new std::thread[nthreads];
        DynamicSequencer<UndirectedGraph::vertex_t>  sequencer(0, graph->number_of_vertices(), nthreads);
        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i] = std::thread( [this, &sequencer, &selector] {populate_root_and_range_sampler_mt(sequencer, selector);});

        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
    }

    for (UndirectedGraph::vertex_t u = 0; u < graph->number_of_vertices(); u++)
        root_sampler->set(u, range_samplers[u]->get_total_length());

    root_sampler->build();

}

void TreeletSampler::populate_root_and_range_sampler_mt(DynamicSequencer<UndirectedGraph::vertex_t> &sequencer, const TreeletStructureSelector* const selector)
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
    if(use_selector)
    {
        delete root_sampler;
        for(UndirectedGraph::vertex_t u=0; u<graph->number_of_vertices(); u++)
            delete range_samplers[u];

        delete[] range_samplers;
    }
}

void TreeletSampler::populate_buffer(DecompositionFIFOBuffer &buffer, const UndirectedGraph::vertex_t u, const Treelet& t, Random *rng)
{
    Treelet split = t.split_child();
    assert(!split.is_colored());

    TreeletTable *table = table_collection->get_table(t.number_of_vertices());
    TreeletTable *split_table = table_collection->get_table(split.number_of_vertices());
    TreeletTable *complement_table = table_collection->get_table(t.number_of_vertices()-split.number_of_vertices());

    TreeletTable::treelet_count_t count = table->get_count(u, t);
    if(count==0)
        return;

    safe_mul(count, t.normalization_factor(), &count);
    auto indices = new TreeletTable::treelet_count_t[buffer_size];
    for(uint32_t i=0; i<buffer_size; i++)
        indices[i] = rng->random_uint<TreeletTable::treelet_count_t>(0,  count-1);

    std::sort(indices, indices+buffer_size);
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
            TreeletTable::treelet_count_t c = complement_table->get_count(u, complement);

            assert(it.count()!=0);
            assert(c*it.count() <= count);

            seen += c*it.count();

            while(found!=buffer_size && indices[found]<seen)
            {
                found++;
                buffer.push( v, t2 );
            }

            if(found==buffer_size)
                break;
        }
    }

    delete[] indices;
}


bool TreeletSampler::sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence, Random *rng)
{
    static thread_local std::map< std::pair<UndirectedGraph::vertex_t , Treelet>, DecompositionFIFOBuffer > buffers;

    assert(t.is_valid());

    *occurrence = u;

    if(t.number_of_vertices() == 1)
        return true;

    const UndirectedGraph::vertex_t degree = graph->degree(u);
    if(buffer_size!=0 && degree >= buffer_degree)
    {
        //Try to use cache
        const auto &[buffer_it, inserted] = buffers.emplace(std::make_pair(u, t), buffer_size);
        auto &buffer = buffer_it->second;

        if(buffer.empty())
            populate_buffer(buffer, u, t, rng);

        assert(!buffer.empty());
        const auto &[child_vertex, child_treelet] = buffer.pop();

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
    TreeletTable *complement_table = table_collection->get_table(t.number_of_vertices()-split.number_of_vertices());

    TreeletTable::treelet_count_t count = table->get_count(u, t);
    if(count==0)
        return false;

    safe_mul(count, t.normalization_factor(), &count);
    TreeletTable::treelet_count_t r = rng->random_uint<TreeletTable::treelet_count_t>(0,  count-1);

    Treelet parent_treelet = invalid_treelet;
    Treelet child_treelet = invalid_treelet;
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
            TreeletTable::treelet_count_t c = complement_table->get_count(u, complement);

            assert(it.count()!=0);
            assert(c*it.count() <= count);

#ifndef NDEBUG
            count -= c*it.count();
            //We already fouund a treelet, but we are still iterating to check that the count matches
            //The next if already matched once, and it would match again. Skip to next iteration.
            if(child_treelet.is_valid())
                continue;
#endif
            if (r >= c * it.count())
                r -= c*it.count();
            else
            {
                parent_treelet = complement;
                child_treelet = t2;
                child_vertex = v;
#ifdef NDEBUG
                goto end_loop; //Children found, exit early
#endif
            }
        }
    }

#ifdef NDEBUG
    end_loop:
#endif

    assert(count == 0);
    assert(child_treelet.is_valid());
    assert(parent_treelet.is_valid());

    return ( parent_treelet.is_singleton() || sample_rooted_occurrence(parent_treelet, u, occurrence + child_treelet.number_of_vertices(), rng) ) &&
            sample_rooted_occurrence(child_treelet, child_vertex, occurrence+1, rng);
}




