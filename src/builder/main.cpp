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

#include <cstdlib>
#include <limits>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>
#include "config.h"
#include "../common/util.h"
#include "../common/OptionsParser.h"
#include "../common/graph/UndirectedGraph.h"
#include "Size1Builder.h"
#include "SequentialBuilder.h"
#include "MultithreadedBuilder.h"
#include "../common/io/PropertyStore.h"

struct builder_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint8_t colors;
    char tables_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char output_basename[MOTIVO_ARG_MAX];
    bool store0;
    char selective_filename[MOTIVO_ARG_MAX];
    double coloring_bias;
};

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
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1, ignored if size=1)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");
    OptionsParser::Option *store0_opt  = op.add_option(false, false, "store-on-0-colored-vertices-only", '0', "", "Store treelet counts only for the vertices with color 0 (default: false, ignored if size=1)");
    OptionsParser::Option *selective_opt = op.add_option(false, true, "selective", '\0', "", "Count only treelets whose structures are allowed in file ARG");
    OptionsParser::Option *coloring_bias_opt = op.add_option(false, true, "coloring-bias", '\0', "1", "Cut the k-colorful probability by a given factor, by reducing the weight of the first k/2 colors.");


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
        uint64_t from = std::stoull(from_opt->get_value());
        if(from >=G.number_of_vertices())
            throw std::runtime_error("'from-vertex' option specifies an invalid vertex");

        opts->from_vertex = static_cast<UndirectedGraph::vertex_t>(from);
    }

    opts->to_vertex = G.number_of_vertices()-1;
    if(to_opt->is_found())
    {
        uint64_t to = std::stoull(to_opt->get_value());
        if(to >=G.number_of_vertices())
            throw std::runtime_error("'to-vertex' option specifies an invalid vertex");

        opts->to_vertex = static_cast<UndirectedGraph::vertex_t>(to);
    }

    if(opts->from_vertex>opts->to_vertex)
        throw std::runtime_error("'from-vertex' and 'to-vertex' options specify an empty range");

    if(opts->size==1)
        opts->threads=1;
    else
    {
        int threads = std::stoi(threads_opt->get_value());
        if (threads < 0)
            throw std::runtime_error("The number of threads is invalid");

        if (threads == 0)
            opts->threads = std::thread::hardware_concurrency();
        else
            opts->threads = static_cast<unsigned int>(threads);

        if (opts->threads <= 0)
            throw std::runtime_error("Failed to determine the number of logical processors");
    }

    if(output_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    strcpy(opts->output_basename, output_opt->get_value().c_str());

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

    opts->coloring_bias = std::stod(coloring_bias_opt->get_value());
    if(!std::isnormal(opts->coloring_bias) || opts->coloring_bias>1 || opts->coloring_bias<=0)
        throw std::runtime_error("'coloring-bias' must be between 0 (exclusive) and 1 (inclusive)");

    return true;
}

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build. Version: " << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

    static builder_opts opts;
    try
    {
        if(!parse_builder_args(argc, argv, "motivo-build", &opts))
            return EXIT_SUCCESS;

        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        double* color_distribution = nullptr;
        if(!double_equality(opts.coloring_bias, 1))
        {
            color_distribution = new double[opts.colors];
            int lpcn = opts.colors - 1; // number of low-probability colors
            double lpc = std::min(1.0 * lpcn / opts.colors, opts.coloring_bias * lpcn); // aggregate prob of the first lpcn colors
            bimodal_distribution(color_distribution, opts.colors, lpcn, lpc);
            std::cout << "color 0 has probability " << color_distribution[0] << std::endl;
            std::cout << "k-colorful probability=" << pcold(color_distribution, opts.colors) << std::endl;

            if(color_distribution[0] < 100.0 / G.number_of_vertices())
                std::cerr << "Warning! Less than 100 nodes in expectation with color 0" << std::endl;
        }

        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>* readers = nullptr;
        TreeletTable** tables = nullptr;
        if(opts.size != 1)
        {
            std::cout << "Loading tables for smaller sizes" << std::endl;
            readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>[opts.size-1];
            tables = new TreeletTable*[opts.size-1];

            for(unsigned int i=0; i<opts.size-1; i++)
            {
                readers[i].open( std::string(opts.tables_basename) + "." + std::to_string(i+1) + ".dtz" );
                readers[i].prefault(opts.from_vertex, opts.to_vertex);
                tables[i] = new TreeletTable(&readers[i]);
                ttc.add(tables[i]);
            }
        }

        const std::string filename = std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".cnt";
        std::ofstream out(filename , std::ofstream::binary | std::ofstream::trunc);
        if(out.bad())
            throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing counts of treelets of size " << opts.size << " for vertices " << opts.from_vertex << "--"
                  << opts.to_vertex << " using " << opts.threads << " thread(s)" << std::endl;

        bool selective = *opts.selective_filename!='\0' && opts.size>1;
        TreeletStructureSelector* selector = nullptr;
        if(selective)
        {
            selector = new TreeletStructureSelector(TreeletStructureSelector(opts.selective_filename).restrict_to_sizes(opts.size,opts.size));
            std::cout << "Selectively " << ((selector->get_mode()==TreeletStructureSelector::MODE_INCLUDE)?"counting only ":"ignoring ") << selector->size() << " treelet(s) of the given size" << std::endl;
        }

        std::chrono::time_point<std::chrono::steady_clock> tstart;
        if(opts.size==1)
        {
            Random rng(opts.seed);
            Size1Builder builder(G.number_of_vertices(), opts.from_vertex, opts.to_vertex, opts.colors, color_distribution, &rng, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else if(opts.threads==1)
        {
            SequentialBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size, &ttc, opts.store0, selector, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else
        {
            MultithreadedBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size, &ttc, opts.store0, selector, &out, opts.threads);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

        std::cerr << "Building time: " << delta_t.count() << " s\n";

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        // write info for later phases
        PropertyStore properties;
        properties.set_bool("StoreOnlyOn0", opts.store0);

        if(color_distribution!= nullptr)
            properties.set_double("ColoringProbability", pcold(color_distribution, opts.colors));

        if(opts.size==1)
            properties.set_uint8("NumberOfColors", opts.colors);

        properties.save(std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".info");

        delete selector;

        for(unsigned int i=0; i<opts.size-1; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
