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
    bool graphlets;
    bool norejection;
    bool footprints;
    bool spanning_trees;
    bool vertices;
    bool smart_stars;
    bool estimate_occurrences;
    bool adaptive;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char selective_filename[MOTIVO_ARG_MAX];
    double time_budget;
    std::string sptrees_file;
};

bool parse_sampler_args(const int argc, const char **argv, const std::string &name, sampler_opts *opts);

#endif //MOTIVO_SAMPLER_OPTS_H
