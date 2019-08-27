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
        //Thread i cleanly finished processing a vertex
        if(phase1_states[i].current_vertex>to_vertex)
            continue;

        assert(missing_vertices<nthreads-1);

        phase2_states[missing_vertices].vertex = phase1_states[i].current_vertex;
        phase2_states[missing_vertices].edges_to_process = phase1_states[i].edges_to_process;

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

    //Find the first vertex to process
    state.current_vertex = next_vertex.fetch_add(1);
    state.next_edge = 0;

    if(state.current_vertex > to_vertex) //Are we already out of vertices?
        for(unsigned int i=0; i<nthreads; i++)
            states[i].terminate_flag=true;
    else
        state.edges_to_process = (store_only_0 && ttc->get_table(1)->begin(state.current_vertex).treelet().get_colors() != 1)?0:G->degree(state.current_vertex);

    while(!state.terminate_flag)
    {
        if(state.next_edge<state.edges_to_process) //There is still some work to do on this vertex
        {
            builder.combine(state.current_vertex, G->neighbor(state.current_vertex, state.next_edge), state.table);
            state.next_edge++;
        }
        else //The vertex is complete
        {
            //Write the vertex table
            std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(state.current_vertex, state.table);
            writer->write(to_write.first, to_write.second);
            state.table.clear();

            //Move to next vertex
            state.current_vertex = next_vertex.fetch_add(1);
            state.next_edge = 0;

            //Are we out of vertices?
            if(state.current_vertex > to_vertex)
            {
                //Signal the other threads to terminate and move to Phase 2
                for(unsigned int i=0; i<nthreads; i++)
                    states[i].terminate_flag=true;
            }
            else
                state.edges_to_process = (store_only_0 && ttc->get_table(1)->begin(state.current_vertex).treelet().get_colors() != 1)?0:G->degree(state.current_vertex);
        }
    }
}

void MultithreadedBuilder::phase2_thread_loop(const unsigned int thread_no, phase2_vertex_state_t *states, const unsigned int nstates, ConcurrentWriter *writer)
{
    for(unsigned int i=0; i<nstates; i++) //Look for a vertex where there is some work to do
    {
        phase2_vertex_state_t &state = states[(thread_no+i)%nstates];

        // We pretend that there are state.edges_to_process + 1 "virtual edges" to process
        // indexed from 0 to state.edges_to_process.
        // The virtual edge indexed with d<state.edges_to_process is the d-th edge of state.vertex (0-indexed)
        // Virtual edge d=state.edges_to_process is a NO-OP and ensures that
        // 1) At least one thread handles state.vertex
        // 2) Each thread that handles state.vertex processes at least one "virtual edge"
        // => Exactly one thread wins the the lottery below and is responsible for writing the tables

        UndirectedGraph::vertex_t d = state.next_edge.fetch_add(1); //Next edge to process
        if(d > state.edges_to_process)  //Is the vertex already fully processed?
            continue;

        unsigned int worker_no = std::numeric_limits<unsigned int>::max();
        if(d<state.edges_to_process)
        {
            //We add ourselves to the number of threads currently working on the vertex
            if(worker_no = state.num_workers.fetch_add(1); worker_no>0)
            {
                assert(worker_no < nthreads);
                //We are not the first thread and we need to do "real" work. Let's create our own Hashmap
                state.tables[worker_no] = new ColorCodingHashmap();
            }
        }

        UndirectedGraph::vertex_t processed_edges=0;
        //Process all the real edges
        while(d < state.edges_to_process)
        {
            assert(worker_no < std::numeric_limits<unsigned int>::max());
            assert(d < G->degree(state.vertex));
            builder.combine(state.vertex, G->neighbor(state.vertex, d), *state.tables[worker_no]);
            processed_edges++;
            d = state.next_edge.fetch_add(1);
        }

        //If we are responsible for the last virtual edge, mark it as done
        if(d == state.edges_to_process)
            processed_edges++; //There is no need to increment state.next_edge as it must be at least state.edges_to_process+1

        //Lottery: every thread reports its processed virtual edges (>=1) sequentially. The last one to report wins.
        processed_edges += state.processed_edges.fetch_add(processed_edges); //Total number of processed virtual edges on this vertex
        assert(processed_edges <= state.edges_to_process+1);
        if(processed_edges == state.edges_to_process+1)
        {
            //We are the winner. All other threads must have already finished processing state.vertex
            merge_and_write(writer, &state); //We take care of merging all tables and writing the result
        }
    }
}

MultithreadedBuilder::MultithreadedBuilder(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex,
                                                       UndirectedGraph::vertex_t to_vertex, const unsigned int size,
                                                       const TreeletTableCollection *ttc, const bool store_only_0,
                                                       TreeletStructureSelector *selector, std::ostream *output,
                                                       unsigned int nthreads)
        : G(G), from_vertex(from_vertex), to_vertex(to_vertex), ttc(ttc), store_only_0(store_only_0),
          output(output), builder(size, ttc, selector), nthreads(nthreads)
{}
