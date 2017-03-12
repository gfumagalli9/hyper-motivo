//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <atomic>
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
        TreeletTable::treelet_count_pair tcp;
        tcp.treelet = Treelet::singleton(coloring->color_of(u));
        tcp.count = 1;
        write_one(u, &tcp, 1);
    }
}

#ifdef MOTIVO_MULTITHREAD
void TreeletTableBuilder::do_build_1_mt(std::atomic<UndirectedGraph::vertex_t> *atomic_cnt)
{
    TreeletTable::treelet_count_pair* processed = new TreeletTable::treelet_count_pair[thread_buffer_size];
    while(true)
    {
        UndirectedGraph::vertex_t start = atomic_cnt->fetch_add(thread_buffer_size);
        if(start>to)
            break;

        UndirectedGraph::vertex_t count = (start+thread_buffer_size<=to)?thread_buffer_size:(to-start+1);
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
        {
            processed[i].treelet = Treelet::singleton(coloring->color_of(start+i));
            processed[i].count = 1;
        }

        write_mutex.lock();
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            write_one(start+i, processed, 1);
        write_mutex.unlock();

    }
    delete[] processed;
}
#endif

void TreeletTableBuilder::do_build_st()
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        table_t table;
        const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
        for (UndirectedGraph::vertex_t d = 0; d < graph->degree(u); d++)
            combine(u, neighbors[d], table);

        TreeletTable::treelet_count_pair* tcp = to_normalized_sorted_array(table);
        write_one(u, tcp, table.size());
        delete[] tcp;
    }
}

#ifdef MOTIVO_MULTITHREAD
void TreeletTableBuilder::do_build_mt(std::atomic<UndirectedGraph::vertex_t> *atomic_cnt)
{
    table_t* tables = new table_t[thread_buffer_size];
    TreeletTable::treelet_count_pair** processed = new TreeletTable::treelet_count_pair*[thread_buffer_size];
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
                combine(u, neighbors[d], tables[i]);

            processed[i] = to_normalized_sorted_array(tables[i]);
        }

        write_mutex.lock();
        for(UndirectedGraph::vertex_t i=0; i<count; i++)
            write_one(start+i, processed[i], tables[i].size());
        write_mutex.unlock();

        for(UndirectedGraph::vertex_t i=0; i<count; i++)
        {
            tables[i].clear(); //FIXME: Clear early?
            delete[] processed[i];
        }
    }

    delete[] tables;
    delete[] processed;
}
#endif

TreeletTable::treelet_count_pair* TreeletTableBuilder::to_normalized_sorted_array(const table_t &table)
{
    TreeletTable::treelet_count_pair *counts = new TreeletTable::treelet_count_pair[table.size()];
    TreeletTable::treelet_count_t i=0;
    table_t::const_iterator u_it = table.begin();
    while(u_it != table.end())
    {
        assert(u_it->second > 0);
        assert(u_it->second % u_it->first.normalization_factor() == 0);
        counts[i].treelet = u_it->first;
        counts[i].count = u_it->second / counts[i].treelet.normalization_factor();

        u_it++;
        i++;
    }

    std::sort(counts, counts+table.size(), [](const TreeletTable::treelet_count_pair& tc1, const TreeletTable::treelet_count_pair& tc2) { return tc1.treelet < tc2.treelet; } );

    return counts;
}

void TreeletTableBuilder::combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts)
{
    for(unsigned int size1=1; size1<size; size1++)
    {
        const TreeletTable* u_table = lower->get_table(size1);
        const TreeletTable* v_table = lower->get_table(size-size1);

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
                    safe_mul(u_it.count(), v_it.count(), &tmp);
                    safe_add(count, tmp, &count);
                }
                else if(merged == Treelet::invalid_merge_structure)
                    break; //All the remaining treelets t2 will have a structure that is too small.
            }
        }
    }
}

void TreeletTableBuilder::write_one(const UndirectedGraph::vertex_t vertex, const TreeletTable::treelet_count_pair* counts, const TreeletTable::treelet_count_t ntreelets)
{
    output->write(reinterpret_cast<const char*>(&vertex), sizeof(UndirectedGraph::vertex_t));
    output->write(reinterpret_cast<const char*>(&ntreelets), sizeof(TreeletTable::treelet_count_t));

#ifndef NDEBUG
    for(TreeletTable::treelet_count_t i=0; i<ntreelets; i++)
    {
        assert(counts[i].treelet.is_valid());
        assert(counts[i].count>0);
        assert(i==0 || counts[i-1].treelet<counts[i].treelet);
    }
#endif

    assert(sizeof(TreeletTable::treelet_count_pair)*ntreelets < static_cast< std::make_unsigned<std::streamsize>::type >(std::numeric_limits<std::streamsize>::max()) );
    output->write(reinterpret_cast<const char*>(counts), static_cast<std::streamsize>(sizeof(TreeletTable::treelet_count_pair)*ntreelets));
}

