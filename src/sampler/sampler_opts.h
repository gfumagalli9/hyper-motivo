//
// Created by steven on 8/14/17.
//

#ifndef MOTIVO_SAMPLER_OPTS_H
#define MOTIVO_SAMPLER_OPTS_H

#include <config.h>
#include <cstdint>
#include <string>
#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/TreeletTableCollection.h"

struct sampler_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint64_t number_of_samples;
    char tables_basename[MOTIVO_ARG_MAX];
    char output_basename[MOTIVO_ARG_MAX];
    bool canonicize;
    bool spanning_trees;
    bool smart_stars;
    bool vertices;
    bool graphlets;
    bool group;
    bool estimate_occurrences;
    bool adaptive;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char selective_filename[MOTIVO_ARG_MAX];
    char selective_build_filename[MOTIVO_ARG_MAX];
    double time_budget;
};

bool parse_sampler_args(int argc, const char **argv, const std::string &name, sampler_opts *opts);

#endif //MOTIVO_SAMPLER_OPTS_H
