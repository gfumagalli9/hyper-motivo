//
// Created by steven on 8/13/17.
//

#include <iostream>
#include <limits>
#include <thread>
#include "builder.h"
#include "../common/OptionsParser.h"

bool parse_builder_args(const int argc, const char **argv, const std::string &name, builder_opts *opts)
{
    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the table to build, between 1 and 16 (required)");
    OptionsParser::Option *colors_opt = op.add_option(false, true, "colors", 'c', "0", "Number of colors to use, between 1 and 16 (required if size=1, ignored if size>1)");
    OptionsParser::Option *tables_opt = op.add_option(false, true, "tables-basename", 'i', "", "Basename of table files of smaller size (required if size > 1, ignored if size=1)");
    OptionsParser::Option *from_opt = op.add_option(false, true, "from-vertex", '\0', "", "First vertex (default: 0)");
    OptionsParser::Option *to_opt = op.add_option(false, true, "to-vertex", '\0', "", "Last vertex (default: last vertex if the graph)");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator for the initial coloring (default or empty string: seed from system random device)");
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");
    OptionsParser::Option *progress_opt = op.add_option(false, true, "progress", 'P', "0", "Number of processed vertices between progress reports or 0 for no progress reports (default: 0)");
    OptionsParser::Option *store0_opt  = op.add_option(false, false, "store-on-0-colored-vertices-only", '0', "", "Store treelet counts only for the vertices with color 0 (default: false)");
    OptionsParser::Option *selective_opt = op.add_option(false, true, "selective", '\0', "", "Count only treelets whose structures are allowed in file ARG");

    if (!op.parse(argc, argv) || help_opt->is_found())
    {
        std::cout << name << " [OPTION]..." << std::endl;
        std::cout << "  Builds count tables for use with motivo-merge" << std::endl << std::endl;
        std::cout << op.help() << std::endl;
        return false;
    }

    if(!op.has_required_options())
        throw std::runtime_error("Required options are missing");

    int size = std::stoi(size_opt->get_value());
    if (size < 1 || size > 16)
        throw std::runtime_error("'size' option is invalid");
    opts->size = static_cast<unsigned int>(size);

    int colors = std::stoi(colors_opt->get_value());
    if(size==1 && (!colors_opt->is_found() || colors < 1 || colors > 16))
        throw std::runtime_error("'colors' option missing or invalid");
    opts->colors = static_cast<uint8_t>(colors);

    if(size != 1 && !tables_opt->is_found())
        throw std::runtime_error("'tables-basename' option is required");
    if(tables_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'tables-basename' option is too long");
    strcpy(opts->tables_basename,tables_opt->get_value().c_str());

    if(graph_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'graph' option is too long");
    strcpy(opts->graph,graph_opt->get_value().c_str());

    UndirectedGraph G(graph_opt->get_value());

    opts->from_vertex = 0;
    if(from_opt->is_found())
    {
        int64_t from = std::stoll(from_opt->get_value());
        if(from < 0 || from >=G.number_of_vertices())
            throw std::runtime_error("'from-vertex' option specifies an invalid vertex");

        opts->from_vertex = static_cast<UndirectedGraph::vertex_t>(from);
    }

    opts->to_vertex = G.number_of_vertices()-1;
    if(to_opt->is_found())
    {
        int64_t to = std::stoll(to_opt->get_value());
        if(to < 0 || to >=G.number_of_vertices())
            throw std::runtime_error("'to-vertex' option specifies an invalid vertex");

        opts->to_vertex = static_cast<UndirectedGraph::vertex_t>(to);
    }

    if(opts->from_vertex>opts->to_vertex)
        throw std::runtime_error("'from-vertex' and 'to-vertex' options specify an empty range");

    int threads = std::stoi(threads_opt->get_value());
    if(threads<0)
        throw std::runtime_error("The number of threads is invalid");

    if (threads == 0)
        opts->threads = std::thread::hardware_concurrency();
    else
        opts->threads = static_cast<unsigned int>(threads);

    if(opts->threads<=0)
        throw std::runtime_error("Failed to determine the number of logical processors");

    if(output_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    strcpy(opts->output_basename, output_opt->get_value().c_str());


    long long progress = std::stoll(progress_opt->get_value());
    if(progress<0)
        throw std::runtime_error("Invalid value of option 'progress'");
    opts->progress = static_cast<UndirectedGraph::vertex_t>(progress);

    opts->store0 = store0_opt->is_found();

    if(seed_opt->get_value().length()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'seed' option is too long");
    strcpy(opts->seed, seed_opt->get_value().c_str());

    if(selective_opt->is_found())
    {
        if(selective_opt->get_value().length()>=MOTIVO_ARG_MAX)
            throw std::runtime_error("'selective' option is too long");
        strcpy(opts->selective_filename, selective_opt->get_value().c_str());
    }
    else
        *(opts->selective_filename)='\0';

    return true;
}

