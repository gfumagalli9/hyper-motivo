#include <cstdlib>
#include <iostream>
#include <fstream>

#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"
#include "boost/program_options.hpp"

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    unsigned int size;
    unsigned int colors;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Print help and exit")
            ("g,graph", po::value<std::string>(), "Input graph basename (required)")
            ("s,size",  po::value<unsigned int>(&size)->default_value(0), "Size of the table to build, between 1 and 16 (required)")
            ("c,colors", po::value<unsigned int>(&colors)->default_value(0), "Number of colors to use, between 1 and 16 (required if size > 1, ignored if size=1)")
            ("t,tables-basename", po::value<std::string>(), "Basename of table files of smaller size (required if size > 1, ignored if size=1)")
            ("from-vertex",  po::value<UndirectedGraph::vertex_t>()->default_value(0), "First vertex (default: 0)")
            ("to-vertex", po::value<UndirectedGraph::vertex_t>(), "Last vertex (default: last vertex of the graph)")
            ("o,output", po::value<std::string>(), "Output file");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if(vm.count("help"))
    {
        std::cout << "motivo-build [OPTION]..." << std::endl;
        std::cout << "  Builds count tables for use with motivo-merge" << std::endl << std::endl;
        std::cout << desc << std::endl;

        return EXIT_SUCCESS;
    }

    if(size < 1 || size > 16)
    {
        std::cout << "'size' parameter is missing or invalid" << std::endl;
        return EXIT_FAILURE;
    }

    if(size==1 && (colors < 1 || colors > 16))
    {
        std::cout << "'colors' parameter is missing or invalid" << std::endl;
        return EXIT_FAILURE;
    }

    if(!vm.count("graph"))
    {
        std::cout << "'graph' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }
    std::string graph_filename = vm["graph"].as<std::string>();

    std::string tables_basename;
    if(size != 1)
    {
        if(!vm.count("tables-basename"))
        {
            std::cout << "'colors' parameter is required" << std::endl;
            return EXIT_FAILURE;
        }
        tables_basename = vm["tables-basename"].as<std::string>();
    }

    UndirectedGraph G(graph_filename);
    std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

    unsigned int from_vertex = vm["from-vertex"].as<UndirectedGraph::vertex_t>();
    unsigned int to_vertex = vm.count("to-vertex")?vm["to-vertex"].as<UndirectedGraph::vertex_t>():G.number_of_vertices()-1;
    if(from_vertex>to_vertex || to_vertex>=G.number_of_vertices())
    {
        std::cout << "'from-fertex' and 'to-vertex' specify an empty range or are larger than the number of vertices in the graph" << std::endl;
        exit(EXIT_FAILURE);
    }

    GraphColoring *coloring = nullptr;
    if(size == 1)
    {
        std::cout << "Generating random coloring using " << std::to_string(colors) << " colors" << std::endl;
        coloring = new GraphColoring(G.number_of_vertices(), colors);
    }

    TreeletTableCollection *ttc = nullptr;
    if(size != 1)
    {
        std::cout << "Loading tables for smaller sizes" << std::endl;
        ttc = new TreeletTableCollection(tables_basename, size - 1);
    }

    if(!vm.count("output"))
    {
        std::cout << "'output' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }

    std::string output_filename = vm["output"].as<std::string>() + ".cnt";
    std::ofstream out(  output_filename, std::ofstream::binary | std::ofstream::trunc);
    if(out.bad())
    {
        std::cout << "Could not open output file for writing" << std::endl;
        delete coloring;
        delete ttc;
        return EXIT_FAILURE;
    }

    std::cout << "Computing treelet counts for vertices " << from_vertex << "--" << to_vertex << std::endl;

    TreeletTableBuilder builder(&G, coloring, size, ttc, &out);
    builder.build(from_vertex, to_vertex);
    out.close();

    std::cout << "Output written to " << output_filename << std::endl;

    delete coloring;
    delete ttc;
    return EXIT_SUCCESS;
}