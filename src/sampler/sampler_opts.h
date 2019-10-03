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
    bool auto_number_of_stars;
    uint64_t number_of_star_samples;
    bool vertices;
    bool graphlets;
    bool group;
    bool estimate_occurrences;
    bool adaptive;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    uint32_t treelet_buffer_size;
    UndirectedGraph::vertex_t treelet_buffer_degree;
    char selective_filename[MOTIVO_ARG_MAX];
    char selective_build_filename[MOTIVO_ARG_MAX];
    double time_budget;
};

bool parse_sampler_args(int argc, const char **argv, const std::string &name, sampler_opts *opts);

#endif //MOTIVO_SAMPLER_OPTS_H
