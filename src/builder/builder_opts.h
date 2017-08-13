//
// Created by steven on 8/13/17.
//

#ifndef MOTIVO_BUILDER_OPTS_H
#define MOTIVO_BUILDER_OPTS_H

#include <config.h>
#include "../common/UndirectedGraph.h"

struct builder_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    unsigned int colors;
    char tables_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    char seed[MOTIVO_ARG_MAX];
    unsigned int threads;
    char output_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t progress;
    bool store0;
    UndirectedGraph::vertex_t batch_size;
};

bool parse_builder_args(const int argc, const char **argv, const std::string &name, builder_opts *opts);



#endif //MOTIVO_BUILDER_OPTS_H
