//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <thread>

#include "TreeletTableBuilder.h"

TreeletTableBuilder::TreeletTableBuilder(const UndirectedGraph* graph, const GraphColoring* coloring, const unsigned int size,
    const TreeletTableCollection* lower, std::ostream* output, sequencer_t* const sequencer, const bool store_0_only, const unsigned int num_threads)
        : graph(graph), coloring(coloring), size(size), lower(lower), output(output), sequencer(sequencer),
          store_0_only(store_0_only), number_of_threads(num_threads)
{
    if(num_threads==0)
        throw std::runtime_error("Invalid number of threads");

    if(size==0)
        throw std::runtime_error("Invalid size");
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
            ConcurrentWriter *writer = new ConcurrentWriter(output, 100*number_of_threads);

            std::thread *worker_threads = new std::thread[number_of_threads];
            for (unsigned int i = 0; i < number_of_threads; i++)
                worker_threads[i] = std::thread([this, writer] { do_build_mt(writer); });

            for (unsigned int i = 0; i < number_of_threads; i++)
                worker_threads[i].join();

            delete[] worker_threads;
            delete writer;
        }
    }
}

void TreeletTableBuilder::do_build_1_st()
{
    constexpr std::streamsize buf_size = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + sizeof(TreeletTable::treelet_count_pair);
    char buffer[buf_size];

    constexpr uint64_t one=1;
    memcpy(buffer+sizeof(UndirectedGraph::vertex_t), &one, sizeof(uint64_t));

    TreeletTable::treelet_count_pair tcp;
    tcp.count=1;

    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if (batch.from >= batch.to)
            break;

        for (UndirectedGraph::vertex_t u = batch.from; u<batch.to; u++)
        {
            if (store_0_only && coloring->color_of(u) != 1) //color 0 is represented as 1<<0 = 1
                continue;

            memcpy(buffer, &u, sizeof(UndirectedGraph::vertex_t));
            tcp.treelet = Treelet::singleton(coloring->color_of(u));
            memcpy(buffer + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t), &tcp,
                   sizeof(TreeletTable::treelet_count_pair));
            output->write(buffer, buf_size);
        }
    }
}

void TreeletTableBuilder::do_build_st()
{
    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if(batch.from>=batch.to)
            break;

        for (UndirectedGraph::vertex_t u = batch.from; u<batch.to; u++)
        {
            if (store_0_only &&
                lower->get_table(1)->begin(u).treelet().get_colors() != 1) //color 0 is represented as 1<<0 = 1
                continue;

            table_t table;
            MOTIVO_INIT_HASHMAP(table);
            const UndirectedGraph::vertex_t degree = graph->degree(u);
            for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
                combine(u, graph->neighbor(u, d), table);

            std::pair<char*, std::size_t> to_write = to_normalized_sorted_byte_array(u, table);
            output->write(to_write.first, static_cast<std::streamsize>(to_write.second));
            delete[] to_write.first;
        }
    }
}

void TreeletTableBuilder::do_build_mt(ConcurrentWriter* writer)
{
    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if(batch.from>=batch.to)
            break;

        for(UndirectedGraph::vertex_t u=batch.from; u<batch.to; u++)
        {
            if(store_0_only && lower->get_table(1)->begin(u).treelet().get_colors()!=1) //color 0 is represented as 1<<0 = 1
                continue;

            table_t table;
            MOTIVO_INIT_HASHMAP(table);
            const UndirectedGraph::vertex_t degree = graph->degree(u);
            for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
                combine(u, graph->neighbor(u,d), table);

            std::pair<char*, std::size_t > r = to_normalized_sorted_byte_array(u, table);
            writer->write(r.first, r.second);
        }
    }
}

std::pair<char*, std::size_t > TreeletTableBuilder::to_normalized_sorted_byte_array(const UndirectedGraph::vertex_t u, const table_t &table)
{
    std::pair<char*, std::size_t> result;
    result.second = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + table.size() * sizeof(TreeletTable::treelet_count_pair);
    result.first = new char[result.second];

    //Prevent alignment issues (size might get copied to unaligned memory)
    memcpy(result.first, &u, sizeof(UndirectedGraph::vertex_t));
    uint64_t size = table.size();
    memcpy(result.first + sizeof(UndirectedGraph::vertex_t), &size, sizeof(uint64_t));

    //Make sure array is properly aligned
    static_assert( (sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t)) % alignof(TreeletTable::treelet_count_pair) == 0, "treelet_count_pair not aligned in buffer" );
    TreeletTable::treelet_count_pair *counts = new(result.first+sizeof(UndirectedGraph::vertex_t)+sizeof(uint64_t)) TreeletTable::treelet_count_pair[size];
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
            const Treelet t1 = u_it.treelet();
            assert(t1.is_valid());
            assert(u_it.count() != 0);

            for(TreeletTable::const_iterator v_it = v_table->begin(v); !v_it.is_over(); ++v_it)
            {
                const Treelet t2 = v_it.treelet();
                assert(t2.is_valid());
                assert(v_it.count() != 0);

                Treelet merged = t1.merge(t2);
                if(merged.is_valid())
                {
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
