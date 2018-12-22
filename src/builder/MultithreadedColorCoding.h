//
// Created by steven on 12/21/18.
//

#ifndef MOTIVO_MULTITHREADED_COLOR_CODING_H
#define MOTIVO_MULTITHREADED_COLOR_CODING_H

#include <mutex>
#include <thread>
#include <cassert>
#include "../common/graph/UndirectedGraph.h"
#include "../common/io/ConcurrentWriter.h"
#include "ColorCodingBuilder.h"

class MultithreadedColorCoding //FIXME: store_on_0
{

private:
    struct vertex_info_t
    {
        UndirectedGraph::vertex_t vertex;
        UndirectedGraph::vertex_t next_edge;
        unsigned int assigned_threads;
        unsigned int slot_index;

        ColorCodingBuilder::table_t* tables;
        unsigned int ntables;
    };

    struct thread_state_t
    {
        vertex_info_t *vertex_info = nullptr;
        UndirectedGraph::vertex_t from_vertex = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t to_vertex = UndirectedGraph::INVALID_VERTEX;
        ColorCodingBuilder::table_t* table = nullptr;
    };

    const UndirectedGraph* const G;
    const unsigned int size;
    const TreeletTableCollection* const ttc;
    const bool store_only_0;
    std::ostream* const output;
    const unsigned int nthreads;
    ColorCodingBuilder builder;

    vertex_info_t **slots;
    unsigned int nbusy_slots = 0;
    UndirectedGraph::vertex_t next_vertex =0;
    std::mutex mutex;

public:
    MultithreadedColorCoding(const UndirectedGraph* G, const unsigned int size, const TreeletTableCollection* ttc,
                  bool store_only_0, TreeletSelector* selector, std::ostream* output, unsigned int nthreads)
            : G(G), size(size), ttc(ttc), store_only_0(store_only_0), output(output), nthreads(nthreads), builder(size, ttc, selector)
    {
        slots = new vertex_info_t*[nthreads];
        for(unsigned int i=0; i<nthreads; i++)
        {
            slots[i] = new vertex_info_t();
            slots[i]->tables = new ColorCodingBuilder::table_t[nthreads];

            for(unsigned int j=0; j<nthreads; j++)
                BUILDER_INIT_HASHMAP(slots[i]->tables[j]);
        }

    }

    ~MultithreadedColorCoding()
    {
        for(unsigned int i=0; i<nthreads; i++)
            delete[] slots[i]->tables;

        delete[] slots;
    }

    void update_state(thread_state_t* const state)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if(state->to_vertex!=UndirectedGraph::INVALID_VERTEX) //The thread was working on some vertex
        {
            //Prefer the vertex the thread was currently working on
            assert(state->from_vertex==state->vertex_info->vertex);
            assert(state->vertex_info->next_edge <= G->degree(state->from_vertex));
            if (state->vertex_info->next_edge != G->degree(state->from_vertex))
            {
                state->to_vertex = G->neighbor(state->from_vertex, state->vertex_info->next_edge++);
                return;
            }

            //All the edges of the that vertex have been assigned. Stop working on it.
            assert(state->vertex_info->assigned_threads>0);
            state->vertex_info->assigned_threads--;

            //If the whole vertex has been processed.
            if (state->vertex_info->assigned_threads == 0)
            {
                //Signal the thread that he is responsible for writing the vertex's table(s)
                state->to_vertex = UndirectedGraph::INVALID_VERTEX;
                return;
            }
        }
        else
        {
            if(state->vertex_info != nullptr )
            {
                //The descriptor slot is now free.
                nbusy_slots--;
                slots[nbusy_slots]->slot_index = state->vertex_info->slot_index;
                slots[state->vertex_info->slot_index] = slots[nbusy_slots];

                slots[nbusy_slots] = state->vertex_info;
                state->vertex_info->slot_index = nbusy_slots;
            }
        }

        //Look for a new vertex for the thread
        while(next_vertex<G->number_of_vertices() && (G->degree(next_vertex)==0 || (store_only_0 && ttc->get_table(1)->begin(next_vertex).treelet().get_colors() != 1)) )
            next_vertex++;

        if(next_vertex<G->number_of_vertices()) //A new vertex is available
        {
            //There must be an empty info slot
            assert(nbusy_slots<nthreads);
            vertex_info_t* info = slots[nbusy_slots++];
            info->vertex=next_vertex++;
            info->next_edge=0;
            info->assigned_threads=1;
            info->ntables=1;

            state->vertex_info = info;
            state->from_vertex=info->vertex;
            state->to_vertex=G->neighbor(state->from_vertex, info->next_edge++);
            state->table=info->tables;

            return;
        }

        //All the vertices are being worked on. Go help some other thread.
        unsigned int slot=0;
        while(slot<nbusy_slots && slots[slot]->next_edge == G->degree(slots[slot]->vertex))
            slot++;

        assert(slot==nbusy_slots || slots[slot]->next_edge < G->degree(slots[slot]->vertex));

        if(slot==nbusy_slots) //All other threads are on their last edge... nothing to do.
        {
            state->from_vertex = UndirectedGraph::INVALID_VERTEX; //Signal the thread to terminate
            return;
        }

        state->vertex_info = slots[slot];
        state->vertex_info->assigned_threads++;
        state->from_vertex = state->vertex_info->vertex;
        state->to_vertex = state->vertex_info->next_edge++;
        state->table = state->vertex_info->tables + state->vertex_info->ntables;
        state->vertex_info->ntables++;
    }

    void thread_loop(ConcurrentWriter *writer)
    {
        thread_state_t state;
        update_state(&state);

        while(state.from_vertex != UndirectedGraph::INVALID_VERTEX)
        {
            if(state.to_vertex != UndirectedGraph::INVALID_VERTEX)
                builder.combine(state.from_vertex, state.to_vertex, *state.table);
            else
                merge_and_write(writer, state.vertex_info);

            update_state(&state);
        }
    }

    void merge_and_write(ConcurrentWriter *writer, vertex_info_t* info)
    {
        //Merge tables
        ColorCodingBuilder::table_t &table = info->tables[0];
        for(unsigned int i=1; i<info->ntables; i++)
        {
            for(const auto&  tcp : info->tables[i])
                table[tcp.first]+=tcp.second;

            info->tables[i].clear();
        }

        auto to_write = builder.to_normalized_sorted_byte_array(info->vertex, table);
        table.clear();
        writer->write(to_write.first, to_write.second);
    }

    void build()
    {
        UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
        output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

        auto *writer = new ConcurrentWriter(output, 100*nthreads);
        auto *worker_threads = new std::thread[nthreads];
        for (unsigned int i = 0; i<nthreads; i++)
            worker_threads[i] = std::thread([this, writer] { thread_loop(writer); });

        for (unsigned int i = 0; i<nthreads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
        delete writer;
    }
};


#endif //MOTIVO_MULTITHREADED_COLOR_CODING_H
