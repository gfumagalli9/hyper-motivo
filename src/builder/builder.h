//
// Created by steven on 8/13/17.
//

#ifndef MOTIVO_BUILDER_H
#define MOTIVO_BUILDER_H

#include <chrono>
#include <config.h>
#include "../common/graph/UndirectedGraph.h"

struct builder_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    unsigned int colors;
    char tables_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char output_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t progress;
    bool store0;
    //UndirectedGraph::vertex_t batch_size;
};

bool parse_builder_args(const int argc, const char **argv, const std::string &name, builder_opts *opts);

double timing(const std::string& name, const std::chrono::time_point<std::chrono::steady_clock> tstart, const std::chrono::time_point<std::chrono::steady_clock> tend, const UndirectedGraph::vertex_t vstart, const  UndirectedGraph::vertex_t vend);
void report_progress(UndirectedGraph::vertex_t next, const UndirectedGraph::vertex_t from_vertex, const UndirectedGraph::vertex_t to_vertex);


#endif //MOTIVO_BUILDER_H
