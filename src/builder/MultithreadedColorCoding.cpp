//
// Created by steven on 12/21/18.
//

#include "MultithreadedColorCoding.h"

MultithreadedColorCoding::MultithreadedColorCoding(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex,
                                                   UndirectedGraph::vertex_t to_vertex, const unsigned int size,
                                                   const TreeletTableCollection *ttc, bool store_only_0,
                                                   TreeletSelector *selector, std::ostream *output, unsigned int nthreads)
        : G(G), from_vertex(from_vertex), to_vertex(to_vertex), size(size), ttc(ttc), store_only_0(store_only_0),
          output(output), nthreads(nthreads), builder(size, ttc, selector)
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

MultithreadedColorCoding::~MultithreadedColorCoding()
{
    for(unsigned int i=0; i<nthreads; i++)
        delete[] slots[i]->tables;

    delete[] slots;
}

void MultithreadedColorCoding::update_state(MultithreadedColorCoding::thread_state_t *const state)
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
        if(state->vertex_info != nullptr)
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
    while(next_vertex<=to_vertex && (G->degree(next_vertex)==0 || (store_only_0 && ttc->get_table(1)->begin(next_vertex).treelet().get_colors() != 1)) )
        next_vertex++;

    if(next_vertex<=to_vertex) //A new vertex is available
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
    state->to_vertex = G->neighbor(state->from_vertex, state->vertex_info->next_edge++);
    state->table = state->vertex_info->tables + state->vertex_info->ntables;
    state->vertex_info->ntables++;
}

void MultithreadedColorCoding::thread_loop(ConcurrentWriter *writer)
{
    thread_state_t state;
    update_state(&state);

    while(state.from_vertex != UndirectedGraph::INVALID_VERTEX)
    {
        if(state.to_vertex != UndirectedGraph::INVALID_VERTEX)
        {
            assert(state.to_vertex  != state.from_vertex);

            mutex.lock();
            mutex.unlock();
            builder.combine(state.from_vertex, state.to_vertex, *state.table);
        }
        else
        {
            merge_and_write(writer, state.vertex_info);
            mutex.lock();
            mutex.unlock();
        }

        update_state(&state);
    }
}

void MultithreadedColorCoding::merge_and_write(ConcurrentWriter *writer, MultithreadedColorCoding::vertex_info_t *info)
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

void MultithreadedColorCoding::build()
{
    UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    next_vertex = from_vertex;

    auto *writer = new ConcurrentWriter(output, 100*nthreads);
    auto *worker_threads = new std::thread[nthreads];
    for (unsigned int i = 0; i<nthreads; i++)
        worker_threads[i] = std::thread([this, writer] { thread_loop(writer); });

    for (unsigned int i = 0; i<nthreads; i++)
        worker_threads[i].join();

    delete[] worker_threads;
    delete writer;
}
