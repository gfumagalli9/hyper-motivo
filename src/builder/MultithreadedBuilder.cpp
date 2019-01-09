//
// Created by steven on 12/21/18.
//

#include "MultithreadedBuilder.h"

MultithreadedBuilder::MultithreadedBuilder(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex,
                                                   UndirectedGraph::vertex_t to_vertex, const unsigned int size,
                                                   const TreeletTableCollection *ttc, bool store_only_0,
                                                   TreeletSelector *selector, std::ostream *output, unsigned int nthreads)
        : G(G), from_vertex(from_vertex), to_vertex(to_vertex), ttc(ttc), store_only_0(store_only_0),
          output(output), nthreads(nthreads), builder(size, ttc, selector)
{
    slots = new vertex_info_t*[nthreads];
    for(unsigned int i=0; i<nthreads; i++)
    {
        slots[i] = new vertex_info_t();
        slots[i]->tables = new ColorCodingHashmap[nthreads];
    }

}

MultithreadedBuilder::~MultithreadedBuilder()
{
    for(unsigned int i=0; i<nthreads; i++)
        delete[] slots[i]->tables;

    delete[] slots;
}

void MultithreadedBuilder::update_state(MultithreadedBuilder::thread_state_t *const state)
{
    std::lock_guard<std::mutex> lock(mutex);

    //Try to assign this many edges to the thread
    UndirectedGraph::vertex_t batch_size = remaining_edges/(nthreads*1000);
    if(batch_size==0)
        batch_size=1;

    if(state->from_edge !=UndirectedGraph::INVALID_VERTEX) //The thread was working on some batch of edges
    {
        //Prefer the vertex the thread was currently working on
        assert(state->vertex==state->vertex_info->vertex);
        assert(state->vertex_info->next_edge < G->degree(state->vertex) || state->vertex_info->next_edge == UndirectedGraph::INVALID_VERTEX);
        if (state->vertex_info->next_edge < G->degree(state->vertex))
        {
            state->from_edge = state->vertex_info->next_edge;

            if(state->vertex_info->next_edge + batch_size  >= G->degree(state->vertex))
            {
                state->vertex_info->next_edge = UndirectedGraph::INVALID_VERTEX;
                state->to_edge = G->degree(state->vertex);
            }
            else
            {
                state->vertex_info->next_edge += batch_size;
                state->to_edge = state->vertex_info->next_edge;
            }

            assert(remaining_edges >= (state->to_edge - state->from_edge));
            remaining_edges -= (state->to_edge - state->from_edge);
            return;
        }

        //All the edges of the that vertex have been assigned. Stop working on it.
        assert(state->vertex_info->assigned_threads>0);
        state->vertex_info->assigned_threads--;

        //If the whole vertex has been processed.
        if (state->vertex_info->assigned_threads == 0)
        {
            //Signal the thread that he is responsible for writing the vertex's table(s)
            state->from_edge = UndirectedGraph::INVALID_VERTEX;
            return;
        }
    }
    else //The thread is free
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

        state->vertex_info = info;
        state->vertex=info->vertex;
        state->from_edge=0;
        if(batch_size>=G->degree(info->vertex))
        {
            info->next_edge=UndirectedGraph::INVALID_VERTEX;
            state->to_edge = G->degree(state->vertex);
        }
        else
        {
            info->next_edge=batch_size;
            state->to_edge=batch_size;
        }

        info->assigned_threads=1;
        info->ntables=1;
        state->table=info->tables;

        assert(remaining_edges >= (state->to_edge - state->from_edge));
        remaining_edges -= (state->to_edge - state->from_edge);

        return;
    }

    //All the vertices are being worked on. Go help some other thread.
    unsigned int slot=0;
    while(slot<nbusy_slots && slots[slot]->next_edge == UndirectedGraph::INVALID_VERTEX)
        slot++;

    assert(slot==nbusy_slots || slots[slot]->next_edge < G->degree(slots[slot]->vertex));

    if(slot==nbusy_slots) //All other threads are on their last edge... nothing to do.
    {
        state->vertex = UndirectedGraph::INVALID_VERTEX; //Signal the thread to terminate
        return;
    }

    //Assign a single edge
    state->vertex_info = slots[slot];
    state->vertex_info->assigned_threads++;
    state->vertex = state->vertex_info->vertex;

    assert(state->vertex_info->next_edge < G->degree(state->vertex));
    state->from_edge = state->vertex_info->next_edge++;
    state->to_edge = state->vertex_info->next_edge;

    if(state->vertex_info->next_edge == G->degree(state->vertex_info->vertex))
        state->vertex_info->next_edge = UndirectedGraph::INVALID_VERTEX;

    state->table = state->vertex_info->tables + state->vertex_info->ntables;
    state->vertex_info->ntables++;

    remaining_edges--;
}

void MultithreadedBuilder::thread_loop(ConcurrentWriter *writer)
{
    thread_state_t state;
    update_state(&state);

    while(state.vertex != UndirectedGraph::INVALID_VERTEX)
    {
        if(state.from_edge != UndirectedGraph::INVALID_VERTEX)
        {
            assert(state.to_edge > state.from_edge);
            assert(state.to_edge <= G->degree(state.vertex));

            for(UndirectedGraph::vertex_t i=state.from_edge; i<state.to_edge; i++)
                builder.combine(state.vertex, G->neighbor(state.vertex, i), *state.table);
        }
        else
            merge_and_write(writer, state.vertex_info);

        update_state(&state);
    }
}

void MultithreadedBuilder::merge_and_write(ConcurrentWriter *writer, MultithreadedBuilder::vertex_info_t *info)
{
    //Merge tables
    ColorCodingHashmap &table = info->tables[0];
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

void MultithreadedBuilder::build()
{
    UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    next_vertex = from_vertex;
    if(from_vertex==0 && to_vertex==G->number_of_vertices() - 1)
        remaining_edges = 2*G->number_of_edges();
    else
    {
        remaining_edges=0;
        for(UndirectedGraph::vertex_t u=from_vertex; u<=to_vertex; u++)
            remaining_edges += G->degree(u);
    }

    auto *writer = new ConcurrentWriter(output, 100*nthreads);
    auto *worker_threads = new std::thread[nthreads];
    for (unsigned int i = 0; i<nthreads; i++)
        worker_threads[i] = std::thread([this, writer] { thread_loop(writer); });

    for (unsigned int i = 0; i<nthreads; i++)
        worker_threads[i].join();

    assert(remaining_edges==0);

    delete[] worker_threads;
    delete writer;
}
