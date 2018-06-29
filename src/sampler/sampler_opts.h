//
// Created by steven on 8/14/17.
//

#ifndef MOTIVO_SAMPLER_OPTS_H
#define MOTIVO_SAMPLER_OPTS_H

#include <config.h>
#include <cstdint>
#include <string>
#include "../common/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/TreeletTableCollection.h"

struct sampler_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint64_t number_of_samples;
    char tables_basename[MOTIVO_ARG_MAX];
    char output_basename[MOTIVO_ARG_MAX];
    bool text;
    bool canonicize;
    bool graphlets;
    bool norejection;
    bool footprints;
    bool spanning_trees;
    bool vertices;
    bool group;
    bool smart_stars;
    bool adaptive;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char selective_filename[MOTIVO_ARG_MAX];
    uint128_t tot_treelets; // the total number of colored treelets
    bool store_only_0; // whether we only counted the k-treelets rooted at the 0-colored vertices
};

bool parse_sampler_args(const int argc, const char **argv, const std::string &name, sampler_opts *opts);

#endif //MOTIVO_SAMPLER_OPTS_H
