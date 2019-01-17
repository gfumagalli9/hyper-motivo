//
// Created by steven on 1/4/19.
//

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
        UndirectedGraph::vertex_t degree = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t next_edge = UndirectedGraph::INVALID_VERTEX;
        ColorCodingHashmap table;
        std::atomic<bool> terminate_flag {false};
    };

    struct phase2_vertex_state_t
    {
        UndirectedGraph::vertex_t vertex = UndirectedGraph::INVALID_VERTEX;
        UndirectedGraph::vertex_t degree = UndirectedGraph::INVALID_VERTEX;
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
            unsigned int size, const TreeletTableCollection *ttc, bool store_only_0, TreeletSelector *selector,
            std::ostream *output, unsigned int nthreads);

    void phase1_thread_loop [[gnu::hot]] (unsigned int thread_no, phase1_thread_state_t *states, ConcurrentWriter *writer);

    void phase2_thread_loop [[gnu::hot]] (unsigned int thread_no, phase2_vertex_state_t *states, unsigned int nstates,ConcurrentWriter *writer);

    void merge_and_write(ConcurrentWriter *writer, phase2_vertex_state_t *state);

    void build();

};


#endif //MOTIVO_MULTITHREADEDBUILDER_H
