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

#include "Occurrence.h"
#include <iostream>
#include <limits>
#include <cstring>
#include <chrono>
#include <new>
#include <thread>
#include "sampler_opts.h"
#include "../common/OptionsParser.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "TreeletSampler.h"

bool parse_sampler_args(const int argc, const char **argv, const std::string &name, sampler_opts *opts)
{
    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the treelets to sample (required)");
    OptionsParser::Option *numsamples_opt = op.add_option(false, true, "num-samples", 'n', "", "Stop after this number of samples (default: unlimited)");
    OptionsParser::Option *input_opt = op.add_option(true, true, "tables-basename", 'i', "", "Input tables basename (required)");
    OptionsParser::Option *output_opt = op.add_option(false, true, "output", 'o', "", "Output file (default: stdout)");
    OptionsParser::Option *canonicize_opt = op.add_option(false, false, "canonicize", 'c', "", "Output occurrences in canonical format");
    OptionsParser::Option *spanning_opt = op.add_option(false, false, "spanning-trees-no", '\0', "", "Output the number of spanning trees in the sampels treelet/graphlet");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator (default or empty string: seed from system random device)");
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1)");
    OptionsParser::Option *treelet_buffer_size_opt = op.add_option(false, true, "treelet-buffer-size", '\0', "0", "Buffer ARG treelets for vertices with high degree. 0 disables buffering (default: 0)");
    OptionsParser::Option *treelet_buffer_degree_opt = op.add_option(false, true, "treelet-buffer-degree", '\0', "10000", "Degree threshold for treelet buffering (default: 10000)");
    OptionsParser::Option *selective_opt = op.add_option(false, true, "selective", '\0', "", "Sample only treelets whose structures are allowed in file ARG");
    OptionsParser::Option *selective_build_opt = op.add_option(false, true, "selective-build", '\0', "", "The --selective file used when building the tables");
    OptionsParser::Option *smart_stars_opt = op.add_option(false, false, "smart-stars", '\0', "", "Sample star treelets separately and then merge the sample results");
    OptionsParser::Option *num_stars_opt = op.add_option(false, true, "num-stars", '\0', "", "Number of star treelets to sample (ignored if --smart-stars is not specified, default: proportional to the number of stars in the graph)");
    OptionsParser::Option *vertices_opt = op.add_option(false, false, "vertices", '\0', "", "Output the IDs of the sampled vertices");
    OptionsParser::Option *graphlets_opt = op.add_option(false, false, "graphlets", '\0', "", "Sample graphlets occurrences (instead of treelets, implies --vertices)");
    OptionsParser::Option *group_opt = op.add_option(false, false, "group", '\0', "", "Group samples with the same footprint");
    OptionsParser::Option *estimate_occurrences_opt = op.add_option(false, false, "estimate-occurrences", '\0', "", "Estimate the number of occurrences of graphlets in the graph (implies: --graphlets, --spanning-trees-no, --group)"); //FIXME: Can this be used with treelets?
    OptionsParser::Option *adaptive_opt = op.add_option(false, false, "estimate-occurrences-adaptive", '\0', "", "Estimate the number of occurrences of graphlets in the graph using adaptive sampling (implies: --estimate-occurrences,  and --canonicize)");
    OptionsParser::Option *time_budget_opt = op.add_option(false, true, "time-budget", '\0', "", "Time budget in seconds");


    bool parse_ok = op.parse(argc, argv);
    if(!parse_ok || help_opt->is_found())
    {
        std::cout << name << " [OPTION]..." << std::endl;
        std::cout << "  Samples treelets from tables" << std::endl << std::endl;
        std::cout << op.help() << std::endl;
        return false;
    }

    if(!op.has_required_options())
        throw std::runtime_error("Required options are missing");

    int size = std::stoi(size_opt->get_value());
    if(size < 1 || size > 16)
        throw std::runtime_error("'size' option is invalid");
    opts->size = static_cast<unsigned int>(size);

    if(!numsamples_opt->is_found() && !time_budget_opt->is_found())
        throw std::runtime_error("At least one of 'num-samples' and 'time-budget' must be specified");

    opts->number_of_samples = std::numeric_limits<uint64_t>::max();
    if(numsamples_opt->is_found())
        opts->number_of_samples = std::stoull(numsamples_opt->get_value());
    if(opts->number_of_samples == 0)
        throw std::runtime_error("'num-samples' option is invalid");

    if(time_budget_opt->is_found())
    {
        opts->time_budget = std::stod(time_budget_opt->get_value());
        if(!numsamples_opt->is_found())
            opts->number_of_samples = 0;
    }
    else
        opts->time_budget = std::numeric_limits<double>::infinity();

    opts->spanning_trees = spanning_opt->is_found();

    if(input_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'tables-basename' option is too long");
    strcpy(opts->tables_basename, input_opt->get_value().c_str());

    if(graph_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'graph' option is too long");
    strcpy(opts->graph, graph_opt->get_value().c_str());

    if(output_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    strcpy(opts->output_basename, output_opt->get_value().c_str());

    if(seed_opt->get_value().length() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'seed' option is too long");
    strcpy(opts->seed, seed_opt->get_value().c_str());

    int threads = std::stoi(threads_opt->get_value());
    if(threads < 0)
        throw std::runtime_error("The number of threads is invalid");

    if(threads == 0)
        opts->threads = std::thread::hardware_concurrency();
    else
        opts->threads = static_cast<unsigned int>(threads);

    if(opts->threads <= 0)
        throw std::runtime_error("Failed to determine the number of logical processors");

    uint64_t treelet_bs = std::stoull(treelet_buffer_size_opt->get_value());
    if(treelet_bs > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("Invalid treelet buffer size");
    opts->treelet_buffer_size = static_cast<uint32_t>(treelet_bs);

    uint64_t treelet_bd = std::stoull(treelet_buffer_degree_opt->get_value());
    if(treelet_bd > std::numeric_limits<UndirectedGraph::vertex_t>::max())
        throw std::runtime_error("Invalid treelet buffer degree threshold");
    opts->treelet_buffer_degree = static_cast<UndirectedGraph::vertex_t>(treelet_bd);

    if(selective_opt->is_found())
    {
        if(selective_opt->get_value().length() >= MOTIVO_ARG_MAX)
            throw std::runtime_error("'selective' option is too long");

        strcpy(opts->selective_filename, selective_opt->get_value().c_str());
    }
    else
        *(opts->selective_filename) = '\0';

    if(selective_build_opt->is_found())
    {
        if(selective_build_opt->get_value().length() >= MOTIVO_ARG_MAX)
            throw std::runtime_error("'selective-build' option is too long");

        strcpy(opts->selective_build_filename, selective_build_opt->get_value().c_str());
    }
    else
        *(opts->selective_build_filename) = '\0';

    opts->canonicize = canonicize_opt->is_found();
    opts->vertices = vertices_opt->is_found();
    opts->graphlets = graphlets_opt->is_found();
    opts->group = group_opt->is_found();

    opts->smart_stars = smart_stars_opt->is_found();
    if(num_stars_opt->is_found())
    {
        opts->number_of_star_samples = std::stoull(num_stars_opt->get_value());
        if(opts->number_of_star_samples > opts->number_of_samples)
            throw std::runtime_error("--num-stars is larger than --num-samples");
    }
    else
        opts->auto_number_of_stars = true;

    opts->estimate_occurrences = estimate_occurrences_opt->is_found();
    opts->adaptive = adaptive_opt->is_found();

    if(opts->adaptive)
    {
        opts->estimate_occurrences = true;
        opts->canonicize = true;
    }

    if(opts->estimate_occurrences)
    {
        opts->graphlets = true;
        opts->spanning_trees = true;
        opts->group = true;
    }

    if(opts->graphlets)
        opts->vertices = true;

    if(opts->adaptive && *opts->selective_filename != '\0')
        throw std::runtime_error("Selective sampling cannot be used with --estimate-occurrences-adaptive");

    return true;
}
