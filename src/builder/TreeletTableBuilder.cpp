//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <utility>
#include "config.h"


#ifdef MOTIVO_MULTITHREAD
    #include <thread>
#endif

#include "TreeletTableBuilder.h"

void TreeletTableBuilder::build(unsigned int nthreads)
{
    UndirectedGraph::vertex_t num_verts = graph->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    if(nthreads>1)
    {
#ifdef MOTIVO_MULTITHREAD
        std::atomic<UndirectedGraph::vertex_t> atomic_cnt(0);

        std::thread *threads = new std::thread[nthreads];
        for (unsigned int i = 0; i < nthreads; i++)
        {
            if (size == 1)
                threads[i] = std::thread([this, &atomic_cnt] { do_build_1_mt(&atomic_cnt); });
            else
                threads[i] = std::thread([this, &atomic_cnt] { do_build_mt(&atomic_cnt); });
        }

        for (unsigned int i = 0; i < nthreads; i++)
            threads[i].join();

        delete[] threads;
#else
        throw std::runtime_error("Multithread support is not enabled");
#endif
    }
    else
    {
        if(size==1)
            do_build_1_st();
        else
            do_build_st();
    }
}

void TreeletTableBuilder::do_build_1_st()
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        table_t counts;
        Treelet treelet = Treelet::singleton(coloring->color_of(u));
        counts[treelet] = 1;
        write(u, counts);
    }
}

#ifdef MOTIVO_MULTITHREAD
void TreeletTableBuilder::do_build_1_mt(std::atomic<UndirectedGraph::vertex_t> *atomic_cnt)
{
    table_t* processed = new table_t[thread_buffer_size];
    while(true)
    {
        UndirectedGraph::vertex_t start = atomic_cnt->fetch_add(thread_buffer_size);
        if(start>to)
            break;

        UndirectedGraph::vertex_t count = (start+thread_buffer_size<=to)?thread_buffer_size:(to-start+1);
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
        {
            Treelet treelet = Treelet::singleton(coloring->color_of(start+i));
            (processed[i])[treelet] = 1;
        }

        write_mutex.lock();
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            write(start+i, processed[i]);
        write_mutex.unlock();

        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            processed[i].clear();
    }

    delete[] processed;
}
#endif

void TreeletTableBuilder::do_build_st()
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        table_t counts;
        const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
        for (UndirectedGraph::vertex_t d = 0; d < graph->degree(u); d++)
            combine(u, neighbors[d], counts);

        normalize(counts);
        write(u, counts);
    }
}

#ifdef MOTIVO_MULTITHREAD
void TreeletTableBuilder::do_build_mt(std::atomic<UndirectedGraph::vertex_t> *atomic_cnt)
{
    table_t* processed = new table_t[thread_buffer_size];
    while(true)
    {
        UndirectedGraph::vertex_t start = atomic_cnt->fetch_add(thread_buffer_size);
        if(start>to)
            break;

        UndirectedGraph::vertex_t count = (start+thread_buffer_size<=to)?thread_buffer_size:(to-start+1);
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
        {
            const UndirectedGraph::vertex_t u = start+i;
            const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
            for (UndirectedGraph::vertex_t d = 0; d < graph->degree(u); d++)
                combine(u, neighbors[d], processed[i]);

            normalize(processed[i]);
        }

        write_mutex.lock();
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            write(start+i, processed[i]);
        write_mutex.unlock();

        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            processed[i].clear();
    }

    delete[] processed;
}
#endif

void TreeletTableBuilder::combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts)
{
    for(unsigned int size1=1; size1<size; size1++)
    {
        unsigned int size2=size-size1;
        const TreeletTable* u_table = lower->get_table(size1);
        const TreeletTable* v_table = lower->get_table(size2);

        for(TreeletTable::const_iterator u_it = u_table->begin(u); u_it != u_table->end(u); u_it++)
        {
            for(TreeletTable::const_iterator v_it = v_table->begin(v); v_it != v_table->end(v); v_it++)
            {
                const Treelet& t1 = u_it.treelet();
                const Treelet& t2 = v_it.treelet();

                Treelet merged = t1.merge(t2);

                if(merged.is_valid())
                {
                    assert(u_it.count() > 0);
                    assert(v_it.count() > 0);
                    //(*counts[u])[merged] += u_it.count() * v_it.count();

                    TreeletTable::treelet_count_t &count = counts[merged];
                    TreeletTable::treelet_count_t tmp;
                    mul_overflow(u_it.count(), v_it.count(), &tmp);
                    add_overflow(count, tmp, &count);
                }
                else if(merged == Treelet::invalid_merge_structure)
                    break; //All the remaining treelets t2 will have a structure that is too small.
            }
        }
    }
}

void TreeletTableBuilder::normalize(table_t& counts)
{
    for(table_t::iterator u_it = counts.begin(); u_it != counts.end(); u_it++)
    {
        assert(u_it->second % u_it->first.normalization_factor() == 0);
        u_it->second /= u_it->first.normalization_factor();
    }
}

void  TreeletTableBuilder::write(const UndirectedGraph::vertex_t vertex, const table_t &counts)
{
    output->write(reinterpret_cast<const char*>(&vertex), sizeof(UndirectedGraph::vertex_t));
    TreeletTable::treelet_count_t nrecords = counts.size();
    output->write(reinterpret_cast<const char*>(&nrecords), sizeof(TreeletTable::treelet_count_t));

    const uint64_t ntreelets = counts.size();
    auto sorted = new std::pair<Treelet, TreeletTable::treelet_count_t>[ntreelets];
    std::copy(counts.begin(), counts.end(), sorted);
    std::sort(sorted, sorted+ntreelets);

    for(TreeletTable::treelet_count_t i=0; i<ntreelets; i++)
    {
        output->write(reinterpret_cast<const char*>(&sorted[i].first), sizeof(Treelet));
        output->write(reinterpret_cast<const char*>(&sorted[i].second), sizeof(TreeletTable::treelet_count_t));
    }
}

