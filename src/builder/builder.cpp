#include <cstdlib>
#include <iostream>
#include <fstream>

#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"
#include "boost/program_options.hpp"

namespace po = boost::program_options;

int main(const int argc, const char** argv)
{
    std::string graph_basename;
    std::string output_filename;
    unsigned int size;
    unsigned int colors;
    unsigned int from_vertex;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Print help and exit")
            ("graph,g", po::value<std::string>(&graph_basename)->required(), "Input graph basename (required)")
            ("size,s",  po::value<unsigned int>(&size)->required(), "Size of the table to build, between 1 and 16 (required)")
            ("colors,c", po::value<unsigned int>(&colors), "Number of colors to use, between 1 and 16 (required if size=1, ignored if size>1)")
            ("tables-basename,s", po::value<std::string>(), "Basename of table files of smaller size (required if size > 1, ignored if size=1)")
            ("from-vertex", po::value<UndirectedGraph::vertex_t>(&from_vertex)->default_value(0), "First vertex (default: 0)")
            ("to-vertex", po::value<UndirectedGraph::vertex_t>(), "Last vertex (default: last vertex of the graph)")
            ("output,o", po::value<std::string>(&output_filename)->required(), "Output file (required)");

    po::variables_map vm;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

        if(vm.count("help"))
        {
            std::cout << "motivo-build [OPTION]..." << std::endl;
            std::cout << "  Builds count tables for use with motivo-merge" << std::endl << std::endl;
            std::cout << desc << std::endl;

            return EXIT_SUCCESS;
        }

        po::notify(vm);

        if (size < 1 || size > 16)
            throw std::runtime_error("'size' option is invalid");

        if(size==1 && (!vm.count("colors") || colors < 1 || colors > 16))
            throw std::runtime_error("'colors' option missing or invalid");

        std::string tables_basename;
        if(size != 1)
        {
            if(!vm.count("tables-basename"))
                throw std::runtime_error("'tables-basename' option is required");
            tables_basename = vm["tables-basename"].as<std::string>();
        }

        UndirectedGraph G(graph_basename);
        std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        unsigned int to_vertex = vm.count("to-vertex")?vm["to-vertex"].as<UndirectedGraph::vertex_t>():G.number_of_vertices()-1;
        if(from_vertex>to_vertex || to_vertex>=G.number_of_vertices())
            throw std::runtime_error("'from-fertex' and 'to-vertex' options specify an empty range or exceed the number of vertices in the graph");

        std::unique_ptr<GraphColoring> coloring;
        if(size == 1)
        {
            std::cout << "Generating random coloring using " << std::to_string(colors) << " colors" << std::endl;
            coloring = std::make_unique<GraphColoring>(G.number_of_vertices(), colors);
        }

        std::unique_ptr<TreeletTableCollection> ttc;
        if(size != 1)
        {
            std::cout << "Loading tables for smaller sizes" << std::endl;
            ttc = std::make_unique<TreeletTableCollection>(tables_basename, size - 1);
        }

        std::ofstream out(  output_filename, std::ofstream::binary | std::ofstream::trunc);
        if(out.bad())
            throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing treelet counts for vertices " << from_vertex << "--" << to_vertex << std::endl;

        TreeletTableBuilder builder(&G, coloring.get(), size, ttc.get(), &out);
        builder.build(from_vertex, to_vertex);
        out.close();

        std::cout << "Output written to " << output_filename << std::endl;
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}