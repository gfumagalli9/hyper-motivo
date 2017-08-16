//
// Created by steven on 8/14/17.
//

#ifndef MOTIVO_SAMPLER_OPTS_H
#define MOTIVO_SAMPLER_OPTS_H

#include <config.h>
#include <cstdint>
#include <string>
#include "../common/Random.h"
#include "../common/UndirectedGraph.h"
#include "../common/TreeletTableCollection.h"

struct sampler_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint64_t number_of_samples;
    uint64_t number_of_accepted_samples;
    char tables_basename[MOTIVO_ARG_MAX];
    char output_basename[MOTIVO_ARG_MAX];
    bool text;
    bool canonicize;
    bool graphlets;
    bool norejection;
    bool footprints;
    bool spanning_trees;
    bool vertices;
    char seed[MOTIVO_ARG_MAX + 1 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
};

bool parse_sampler_args(const int argc, const char **argv, const std::string &name, sampler_opts *opts);

#endif //MOTIVO_SAMPLER_OPTS_H
