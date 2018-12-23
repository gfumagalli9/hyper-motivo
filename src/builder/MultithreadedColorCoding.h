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
    const UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    const unsigned int size;
    const TreeletTableCollection* const ttc;
    const bool store_only_0;
    std::ostream* const output;
    const unsigned int nthreads;
    ColorCodingBuilder builder;

    vertex_info_t **slots;
    unsigned int nbusy_slots = 0;
    UndirectedGraph::vertex_t next_vertex;
    std::mutex mutex;


    void update_state [[gnu::hot]] (thread_state_t* state);

    void thread_loop [[gnu::hot]] (ConcurrentWriter *writer);

    void merge_and_write(ConcurrentWriter *writer, vertex_info_t* info);

public:
    MultithreadedColorCoding(const UndirectedGraph* G, UndirectedGraph::vertex_t from_vertex, UndirectedGraph::vertex_t to_vertex,
                             const unsigned int size, const TreeletTableCollection* ttc, bool store_only_0,
                             TreeletSelector* selector, std::ostream* output, unsigned int nthreads);

    ~MultithreadedColorCoding();

    void build();
};


#endif //MOTIVO_MULTITHREADED_COLOR_CODING_H
