//
// Created by steven on 8/14/17.
//

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
#include "Occurrence.h"

bool parse_sampler_args(const int argc, const char **argv, const std::string &name, sampler_opts *opts)
{
    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the treelets to sample (required)");
    OptionsParser::Option *numsamples_opt = op.add_option(false, true, "num-samples", 'n', "", "Stop after this number of samples (default: unlimited)");
    OptionsParser::Option *input_opt = op.add_option(true, true, "tables-basename", 'i', "", "Input tables basename (required)");
    OptionsParser::Option *output_opt = op.add_option(false, true, "output", 'o', "", "Output file (default: stdout)");
    OptionsParser::Option *text_opt = op.add_option(false, false, "text", 't', "", "Output occurrences in text format");
    OptionsParser::Option *canonicize_opt = op.add_option(false, false, "canonicize", 'c', "", "Output occurrences in canonical format");
    OptionsParser::Option *graphlets_opt = op.add_option(false, false, "graphlets", '\0', "", "Sample graphlets occurrences (instead of treelets)");
    OptionsParser::Option *norejection_opt = op.add_option(false, false, "no-rejection", '\0', "", "Do not perform rejection on the sampled graphlets");
    OptionsParser::Option *footprints_opt = op.add_option(false, false, "footprints", '\0', "", "Output the graphlet/treelet footprints");
    OptionsParser::Option *spanning_opt = op.add_option(false, false, "spanning-trees-no", '\0', "", "Output the number of spanning trees in the sampels treelet/graphlet");
    OptionsParser::Option *vertices_opt = op.add_option(false, false, "vertices", '\0', "", "Output the IDs of the sampled vertices");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator (default or empty string: seed from system random device)");
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1)");

    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << name << " [OPTION]..." << std::endl;
        std::cout << "  Samples treelets from tables" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return false;
    }

    if(!op.has_required_options())
        throw std::runtime_error("Required options are missing");


    int size = std::stoi(size_opt->get_value());
    if (size < 1 || size > 16)
        throw std::runtime_error("'size' option is invalid");
    opts->size = static_cast<unsigned int>(size);

    if(!numsamples_opt->is_found() && !numsamples_opt->is_found())
        throw std::runtime_error("At least one of 'num-samples' and 'num-accepted' must be specified");

    opts->number_of_samples = std::numeric_limits<uint64_t>::max();
    if(numsamples_opt->is_found())
        opts->number_of_samples = std::stoull(numsamples_opt->get_value());
    if(opts->number_of_samples==0)
        throw std::runtime_error("'num-samples' option is invalid");

    opts->footprints = footprints_opt->is_found();
    opts->spanning_trees = spanning_opt->is_found();
    opts->vertices = vertices_opt->is_found();
    if(!opts->footprints  && !opts->spanning_trees && !opts->vertices)
        throw std::runtime_error("Nothing to output. Please specify at least one of --footprints, --spanning-trees-no, --vertices");

    if(input_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'tables-basename' option is too long");
    strcpy(opts->tables_basename,input_opt->get_value().c_str());

    if(graph_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'graph' option is too long");
    strcpy(opts->graph,graph_opt->get_value().c_str());

    if(output_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    strcpy(opts->output_basename,output_opt->get_value().c_str());

    if(seed_opt->get_value().length()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'seed' option is too long");
    strcpy(opts->seed, seed_opt->get_value().c_str());


    int threads = std::stoi(threads_opt->get_value());
    if(threads<0)
        throw std::runtime_error("The number of threads is invalid");

    if (threads == 0)
        opts->threads = std::thread::hardware_concurrency();
    else
        opts->threads = static_cast<unsigned int>(threads);

    if(opts->threads<=0)
        throw std::runtime_error("Failed to determine the number of logical processors");

    opts->threads = static_cast<unsigned int>(threads);


    opts->canonicize = canonicize_opt->is_found();
    opts->graphlets = graphlets_opt->is_found();
    opts->norejection = norejection_opt->is_found();
    opts->text = text_opt->is_found();

    return true;
}