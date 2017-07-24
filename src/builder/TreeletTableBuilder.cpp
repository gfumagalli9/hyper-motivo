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


TreeletTableBuilder::TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
                    const TreeletTableCollection* lower,  const UndirectedGraph::vertex_t from,
                    const UndirectedGraph::vertex_t to, std::ostream* output, const bool store_0_only, const unsigned int num_threads)
        :  graph(graph), coloring(coloring), size(size), lower(lower), from(from), to(to), output(output),
           progress_callback(nullptr), store_0_only(store_0_only), number_of_threads(num_threads)
#ifdef MOTIVO_MULTITHREAD
        , write_queue(2*num_threads)
#endif
{
    if(num_threads==0)
        throw std::runtime_error("Invalid number of threads");

#ifndef MOTIVO_MULTITHREAD
    if(num_threads!=1)
        throw std::runtime_error("Multithread support is not enabled");
#endif

    //TODO: Check size, and from -- to
}

void TreeletTableBuilder::build()
{
    UndirectedGraph::vertex_t num_verts = graph->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    if(size==1)
        do_build_1_st();
    else
    {
        if( number_of_threads == 1)
            do_build_st();
        else
        {
#ifdef MOTIVO_MULTITHREAD
            std::atomic<UndirectedGraph::vertex_t> atomic_cnt(0);
            std::thread writer_thread([this] { writer_loop(); });

            std::thread *worker_threads = new std::thread[number_of_threads];
            for (unsigned int i = 0; i < number_of_threads; i++)
                worker_threads[i] = std::thread([this, &atomic_cnt] { do_build_mt(&atomic_cnt); });

            for (unsigned int i = 0; i < number_of_threads; i++)
                worker_threads[i].join();

            delete[] worker_threads;

            writer_thread.join();
#endif
        }
    }
}

void TreeletTableBuilder::do_build_1_st()
{
    constexpr const std::streamsize buf_size = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + sizeof(TreeletTable::treelet_count_pair);

    char buffer[buf_size];
    UndirectedGraph::vertex_t* vertex = reinterpret_cast<UndirectedGraph::vertex_t*>(buffer);
    *(reinterpret_cast<uint64_t*>(buffer + sizeof(UndirectedGraph::vertex_t))) = 1;
    TreeletTable::treelet_count_pair* tcp = reinterpret_cast<TreeletTable::treelet_count_pair*>(buffer + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) );
    tcp->count = 1;

    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        report_progress(u);

        if(store_0_only && coloring->color_of(u) != 1) //color 0 is represented as 1<<0 = 1
            continue;

        *vertex=u;
        tcp->treelet = Treelet::singleton(coloring->color_of(u));
        output->write(buffer, buf_size);
    }

}

void TreeletTableBuilder::do_build_st()
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        report_progress(u);

        if(store_0_only && lower->get_table(1)->begin(u).treelet().get_colors()!=1) //color 0 is represented as 1<<0 = 1
            continue;

        table_t table;
        const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
        for (UndirectedGraph::vertex_t d = 0; d < graph->degree(u); d++)
            combine(u, neighbors[d], table);

        std::pair<char*, std::streamsize> to_write = to_normalized_sorted_byte_array(u, table);
        output->write(to_write.first, to_write.second);
        delete[] to_write.first;
    }
}

#ifdef MOTIVO_MULTITHREAD
void TreeletTableBuilder::do_build_mt(std::atomic<UndirectedGraph::vertex_t> *atomic_cnt)
{
    while(true)
    {
        UndirectedGraph::vertex_t start = atomic_cnt->fetch_add(thread_batch_size);
        if(start>to)
            break;

        UndirectedGraph::vertex_t end = (start+thread_batch_size-1<=to)?(start+thread_batch_size-1):to;
        std::pair<char*, std::streamsize>* batch = new std::pair<char*, std::streamsize>[end-start+2];
        batch[end-start+1].second = -1;
        for(UndirectedGraph::vertex_t u=start; u<=end; u++)
        {
            report_progress(u);

            if(store_0_only && lower->get_table(1)->begin(u).treelet().get_colors()!=1) //color 0 is represented as 1<<0 = 1
            {
                batch[u-start] = std::make_pair(nullptr, 0);
                continue;
            }

            table_t table;
            const UndirectedGraph::vertex_t *neighbors = graph->neighbors(u);
            for (UndirectedGraph::vertex_t d = 0; d < graph->degree(u); d++)
                combine(u, neighbors[d], table);

            batch[u-start] = to_normalized_sorted_byte_array(u, table);
        }

        write_queue.push( batch );
    }
}
#endif


std::pair<char*, std::streamsize> TreeletTableBuilder::to_normalized_sorted_byte_array(const UndirectedGraph::vertex_t u, const table_t &table)
{
    std::pair<char*, std::streamsize> result;
    result.second = static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + table.size() * sizeof(TreeletTable::treelet_count_pair));
    result.first = new char[result.second];
    char* p = result.first;

    *(reinterpret_cast<UndirectedGraph::vertex_t*>(p)) = u;
    p += sizeof(UndirectedGraph::vertex_t);

    *(reinterpret_cast<uint64_t*>(p)) = table.size();
    p += sizeof(uint64_t);

    TreeletTable::treelet_count_pair *counts = reinterpret_cast<TreeletTable::treelet_count_pair*>(p);
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

    return result;
}

void TreeletTableBuilder::combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts)
{
    for(unsigned int size1=1; size1<size; size1++)
    {
        TreeletTable* u_table = lower->get_table(size1);
        TreeletTable* v_table = lower->get_table(size-size1);

        for(TreeletTable::const_iterator u_it = u_table->begin(u); !u_it.is_over(); ++u_it)
        {
            for(TreeletTable::const_iterator v_it = v_table->begin(v); !v_it.is_over(); ++v_it)
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

void TreeletTableBuilder::writer_loop()
{
    UndirectedGraph::vertex_t written=from;
    while(written<=to)
    {
        std::pair<char*, std::streamsize>* to_write = write_queue.pop();
        std::pair<char*, std::streamsize>* p = to_write;

        while(p->second!=-1)
        {
            if(p->first)
            {
                output->write(p->first, p->second);
                delete[] p->first;
            }

            p++;
            written++;
        }

        delete[] to_write;
    }
}
