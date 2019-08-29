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

#ifndef MOTIVO_MULTITHREADEDBUILDER_H
#define MOTIVO_MULTITHREADEDBUILDER_H

#include <atomic>
#include "ColorCodingHashmap.h"
#include "ColorCodingBuilder.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/io/ConcurrentWriter.h"

class MultithreadedBuilder
{
private:
    static_assert(std::atomic<bool>::is_always_lock_free, "std::atomic<bool> is not always lock free");
    static_assert(std::atomic<UndirectedGraph::vertex_t>::is_always_lock_free, "std::atomic<UndirectedGraph::vertex_t> is not always lock free");
    static_assert(std::atomic<unsigned int>::is_always_lock_free, "std::atomic<unsigned int> is not always lock free");

    struct phase1_thread_state_t
    {
        UndirectedGraph::vertex_t current_vertex = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t edges_to_process = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t next_edge = UndirectedGraph::INVALID_VERTEX;
        ColorCodingHashmap table;
        std::atomic<bool> terminate_flag {false};
    };

    struct phase2_vertex_state_t
    {
        UndirectedGraph::vertex_t vertex = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t edges_to_process = UndirectedGraph::INVALID_VERTEX;
        std::atomic<UndirectedGraph::vertex_t> next_edge {UndirectedGraph::INVALID_VERTEX};
        std::atomic<UndirectedGraph::vertex_t> processed_edges {UndirectedGraph::INVALID_VERTEX};
        std::atomic<unsigned int> num_workers {0};
        ColorCodingHashmap **tables = nullptr;
    };

    const UndirectedGraph *const G;
    const UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    const TreeletTableCollection *const ttc;
    const bool store_only_0;
    std::ostream *const output;
    ColorCodingBuilder builder;

    unsigned int nthreads;
    std::atomic<UndirectedGraph::vertex_t> next_vertex {UndirectedGraph::INVALID_VERTEX};


public:
    MultithreadedBuilder(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex, UndirectedGraph::vertex_t to_vertex,
            unsigned int size, const TreeletTableCollection *ttc, bool store_only_0, TreeletStructureSelector *selector,
            std::ostream *output, unsigned int nthreads);

    void phase1_thread_loop [[gnu::hot]] (unsigned int thread_no, phase1_thread_state_t *states, ConcurrentWriter *writer);

    void phase2_thread_loop [[gnu::hot]] (unsigned int thread_no, phase2_vertex_state_t *states, unsigned int nstates,ConcurrentWriter *writer);

    void merge_and_write(ConcurrentWriter *writer, phase2_vertex_state_t *state);

    void build();

};


#endif //MOTIVO_MULTITHREADEDBUILDER_H
