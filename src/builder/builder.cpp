#include <iostream>
#include <fstream>

#include "../../libs/cxxopts.hpp"
#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"

int main(int argc, char** argv)
{
    cxxopts::Options options("motivo-build", "Builds count tables for use with motivo-merge", "[OPTIONS...]");

    options.add_options("")
            ("help", "Print help and exit")
            ("g,graph", "Input graph basename (required)", cxxopts::value<std::string>(), "BASENAME")
            ("s,size", "Size of the table to build, between 1 and 16 (required)", cxxopts::value<unsigned int>(), "N")
            ("c,colors", "Number of colors to use, between 1 and 16 (required if size > 1, , ignored if size=1)", cxxopts::value<unsigned int>(), "N")
            ("t,tables-basename", "Basename of table files of smaller size (required if size > 1, ignored if size=1)", cxxopts::value<std::string>(), "BASENAME")
            ("from-vertex", "First vertex (default: 0)", cxxopts::value<UndirectedGraph::vertex_t>(), "N")
            ("to-vertex", "Last vertex (default: last vertex of the graph)", cxxopts::value<UndirectedGraph::vertex_t>(), "N")
            ("o,output", "Output file", cxxopts::value<std::string>(), "FILE");

    options.parse(argc, argv);

    if(options.count("help"))
    {
        std::cout << options.help() << std::endl;
        return EXIT_SUCCESS;
    }

    unsigned int size = options["size"].as<unsigned int>();
    if(size < 1 || size > 16)
    {
        std::cout << "'size' parameter is missing or invalid" << std::endl;
        return EXIT_FAILURE;
    }

    unsigned int colors = options["colors"].as<unsigned int>();
    if(size==1 && (colors < 1 || colors > 16))
    {
        std::cout << "'colors' parameter is missing or invalid" << std::endl;
        return EXIT_FAILURE;
    }

    if(!options["graph"].count())
    {
        std::cout << "'graph' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }
    std::string graph_filename = options["graph"].as<std::string>();

    std::string tables_basename;
    if(size != 1)
    {
        if(!options["tables-basename"].count())
        {
            std::cout << "'colors' parameter is required" << std::endl;
            return EXIT_FAILURE;
        }
        tables_basename = options["tables-basename"].as<std::string>();
    }

    UndirectedGraph G(graph_filename);
    std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

    unsigned int from_vertex = options["from-vertex"].count()?options["from-vertex"].as<UndirectedGraph::vertex_t>():0;
    unsigned int to_vertex = options["to-vertex"].count()?options["to-vertex"].as<UndirectedGraph::vertex_t>():G.number_of_vertices()-1;
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

    if(!options["output"].count())
    {
        std::cout << "'output' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }

    std::string output_filename = options["output"].as<std::string>() + ".cnt";
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