//
// Created by steven on 1/4/19.
//

#include "MultithreadedBuilder.h"

void MultithreadedBuilder::build()
{
    //Write header to output file
    UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    auto *writer = new ConcurrentWriter(output, 100*nthreads);

    //Phase 1
    next_vertex = from_vertex;
    auto phase1_states = new phase1_thread_state_t[nthreads];
    auto phase1_threads = new std::thread[nthreads];
    for (unsigned int i = 0; i<nthreads; i++)
        phase1_threads[i] = std::thread([this, i, phase1_states, writer] { phase1_thread_loop(i, phase1_states, writer); });

    for (unsigned int i = 0; i<nthreads; i++)
        phase1_threads[i].join();

    assert(next_vertex>to_vertex);
    delete[] phase1_threads;
    //End of Phase 1

    //Phase 2
    unsigned int missing_vertices=0;
    auto phase2_states = new phase2_vertex_state_t[nthreads-1];
    for (unsigned int i = 0; i<nthreads; i++)
    {
        if(phase1_states[i].current_vertex==UndirectedGraph::INVALID_VERTEX)
            continue;

        assert(missing_vertices<nthreads-1);

        phase2_states[missing_vertices].vertex = phase1_states[i].current_vertex;
        phase2_states[missing_vertices].degree = phase1_states[i].degree;

        phase2_states[missing_vertices].next_edge = phase1_states[i].next_edge;
        phase2_states[missing_vertices].processed_edges = phase1_states[i].next_edge;
        phase2_states[missing_vertices].num_workers = 0;

        phase2_states[missing_vertices].tables = new ColorCodingHashmap *[nthreads];
        phase2_states[missing_vertices].tables[0] = &phase1_states[i].table;

        missing_vertices++;
    }

    auto phase2_threads = new std::thread[nthreads];
    for (unsigned int i = 0; i<nthreads; i++)
        phase2_threads[i] = std::thread([this, i, missing_vertices, phase2_states, writer] { phase2_thread_loop(i, phase2_states, missing_vertices, writer); });

    for (unsigned int i = 0; i<nthreads; i++)
        phase2_threads[i].join();
    delete[] phase2_threads;

    for (unsigned int i = 0; i<missing_vertices; i++)
        delete[] phase2_states[i].tables;

    delete[] phase2_states;
    delete[] phase1_states;
    //End of Phase 2

    delete writer;
}

void MultithreadedBuilder::merge_and_write(ConcurrentWriter *writer, phase2_vertex_state_t *state)
{
    //Merge tables
    ColorCodingHashmap &table = *state->tables[0];
    for(unsigned int i=1; i<state->num_workers; i++)
    {
        for(const auto&  tcp : *state->tables[i])
            table[tcp.first]+=tcp.second;

        delete state->tables[i];
    }

    auto to_write = builder.to_normalized_sorted_byte_array(state->vertex, table);
    table.clear();
    writer->write(to_write.first, to_write.second);
}

void MultithreadedBuilder::phase1_thread_loop(const unsigned int thread_no, phase1_thread_state_t *states, ConcurrentWriter *writer)
{
    phase1_thread_state_t &state = states[thread_no];

    do
        state.current_vertex = next_vertex.fetch_add(1);
    while( (state.current_vertex <= to_vertex) && ( (state.degree = G->degree(state.current_vertex))==0 || (store_only_0 && ttc->get_table(1)->begin(state.current_vertex).treelet().get_colors() != 1)) );
    state.next_edge = 0;

    if(state.current_vertex > to_vertex)
    {
        state.current_vertex = UndirectedGraph::INVALID_VERTEX;
        return;
    }

    while(!state.terminate_flag)
    {
        builder.combine(state.current_vertex, G->neighbor(state.current_vertex, state.next_edge), state.table);
        state.next_edge++;

        if(state.next_edge==state.degree)
        {
            std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(state.current_vertex, state.table);
            writer->write(to_write.first, to_write.second);
            state.table.clear();

            do
                state.current_vertex = next_vertex.fetch_add(1);
            while( (state.current_vertex <= to_vertex) && ( (state.degree = G->degree(state.current_vertex))==0 || (store_only_0 && ttc->get_table(1)->begin(state.current_vertex).treelet().get_colors() != 1)) );
            state.next_edge = 0;

            if(state.current_vertex > to_vertex)
            {
                state.current_vertex = UndirectedGraph::INVALID_VERTEX;
                for(unsigned int i=0; i<nthreads; i++)
                    states[i].terminate_flag=true;
            }
        }
    }
}

void MultithreadedBuilder::phase2_thread_loop(const unsigned int thread_no, phase2_vertex_state_t *states, const unsigned int nstates, ConcurrentWriter *writer)
{
    for(unsigned int i=0; i<nstates; i++)
    {
        phase2_vertex_state_t &state = states[(thread_no+i)%nstates];

        UndirectedGraph::vertex_t d = state.next_edge.fetch_add(1);
        if(d >= state.degree)
            continue;

        unsigned int worker_no = state.num_workers.fetch_add(1);
        if(worker_no>0)
        {
            assert(worker_no<nthreads);
            state.tables[worker_no] = new ColorCodingHashmap();
        }

        assert(!store_only_0 || ttc->get_table(1)->begin(state.vertex).treelet().get_colors() == 1);

        UndirectedGraph::vertex_t processed_edges=0;
        do
        {
            builder.combine(state.vertex, G->neighbor(state.vertex, d), *state.tables[worker_no]);
            d = state.next_edge.fetch_add(1);
            processed_edges++;
        }
        while(d < state.degree);

        processed_edges += state.processed_edges.fetch_add(processed_edges);
        assert(processed_edges <= state.degree);
        if(processed_edges == state.degree)
            merge_and_write(writer, &state);
    }
}

MultithreadedBuilder::MultithreadedBuilder(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex,
                                                       UndirectedGraph::vertex_t to_vertex, const unsigned int size,
                                                       const TreeletTableCollection *ttc, const bool store_only_0,
                                                       TreeletSelector *selector, std::ostream *output,
                                                       unsigned int nthreads)
        : G(G), from_vertex(from_vertex), to_vertex(to_vertex), ttc(ttc), store_only_0(store_only_0),
          output(output), builder(size, ttc, selector), nthreads(nthreads)
{}
