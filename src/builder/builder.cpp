#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>

#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"
#include "../common/OptionsParser.h"

int main(const int argc, const char** argv)
{

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the table to build, between 1 and 16 (required)");
    OptionsParser::Option *colors_opt = op.add_option(false, true, "colors", 'c', "0", "Number of colors to use, between 1 and 16 (required if size=1, ignored if size>1)");
    OptionsParser::Option *tables_opt = op.add_option(false, true, "tables-basename", 't', "", "Basename of table files of smaller size (required if size > 1, ignored if size=1)");
    OptionsParser::Option *from_opt = op.add_option(false, true, "from-vertex", '\0', "", "First vertex (default: 0)");
    OptionsParser::Option *to_opt = op.add_option(false, true, "to-vertex", '\0', "", "Last vertex (default: last vertex if the graph)");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator for the initial coloring (default or empty string: seed from system random device)");
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1)");
    OptionsParser::Option* output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");


    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-build [OPTION]..." << std::endl;
        std::cout << "  Builds count tables for use with motivo-merge" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return EXIT_SUCCESS;
    }

    if(!op.has_required_options())
    {
        std::cout << "Required options are missing" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        int size = std::stoi(size_opt->get_value());
        if (size < 1 || size > 16)
            throw std::runtime_error("'size' option is invalid");

        int colors = std::stoi(colors_opt->get_value());
        if(size==1 && (!colors_opt->is_found() || colors < 1 || colors > 16))
            throw std::runtime_error("'colors' option missing or invalid");

        if(size != 1 && !tables_opt->is_found())
            throw std::runtime_error("'tables-basename' option is required");

        UndirectedGraph G(graph_opt->get_value());
        std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        unsigned int from_vertex = 0;
        if(to_opt->is_found())
        {
            int64_t from = std::stoll(from_opt->get_value());
            if(from < 0 || from >=G.number_of_vertices())
                throw std::runtime_error("'from-vertex' option specifies an invalid vertex");

            from_vertex = static_cast<UndirectedGraph::vertex_t>(from);
        }

        unsigned int to_vertex = G.number_of_vertices()-1;
        if(to_opt->is_found())
        {
            int64_t to = std::stoll(to_opt->get_value());
            if(to < 0 || to >=G.number_of_vertices())
                throw std::runtime_error("'to-vertex' option specifies an invalid vertex");

            to_vertex = static_cast<UndirectedGraph::vertex_t>(to);
        }

        unsigned int nthreads=1;

        {
            int threads = std::stoi(threads_opt->get_value());
            if (threads == 0)
                nthreads = std::thread::hardware_concurrency();
            else if(threads>0)
                nthreads= static_cast<unsigned int>(threads);
        }
        if(nthreads<=0)
            throw std::runtime_error("The number of threads is invalid or it is not possible to determine the number of logical processors");

        if(from_vertex>to_vertex)
            throw std::runtime_error("'from-fertex' and 'to-vertex' options specify an empty range");

        std::unique_ptr<GraphColoring> coloring;
        if(size == 1)
        {
            std::cout << "Generating random coloring of " << (to_vertex-from_vertex+1) << " vertices using " << std::to_string(colors) << " colors" << std::endl;
            Random rng(seed_opt->get_value());
            std::cout << "Using seed: \"" << rng.get_seed() <<"\"" << std::endl;
            coloring = std::make_unique<GraphColoring>(from_vertex, to_vertex, colors, &rng);
        }

        std::unique_ptr<TreeletTableCollection> ttc;
        if(size != 1)
        {
            std::cout << "Loading tables for smaller sizes" << std::endl;
            ttc = std::make_unique<TreeletTableCollection>(tables_opt->get_value(), size - 1);
        }

        std::ofstream out( output_opt->get_value(), std::ofstream::binary | std::ofstream::trunc);
        if(out.bad())
            throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing counts of treelet of size " << size << " for vertices " << from_vertex << "--"
                  << to_vertex << " using " << nthreads << " thread(s)" << std::endl;

        TreeletTableBuilder builder(&G, coloring.get(), static_cast<unsigned  int>(size), ttc.get(), from_vertex, to_vertex, &out);
        builder.build(nthreads);
        out.close();

        std::cout << "Output written to " << output_opt->get_value() << std::endl;
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}