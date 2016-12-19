//
// Created by steven on 12/3/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "../common/UndirectedGraph.h"

namespace po = boost::program_options;

void graph2bin(const std::string &graph_filename, const std::string &output_basename)
{
    UndirectedGraph::vertex_t num_verts;
    uint32_t num_edges;

    std::ifstream stream(graph_filename);

    std::ofstream offsets(output_basename + ".gof", std::ofstream::binary | std::ofstream::trunc);
    std::ofstream edges(output_basename + ".ged", std::ofstream::binary | std::ofstream::trunc);

    stream >> num_verts >> num_edges;
    offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
    offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(uint32_t));

    UndirectedGraph::vertex_t processed_edges = 0;
    for(UndirectedGraph::vertex_t u=0; u < num_verts; u++)
    {
        offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

        UndirectedGraph::vertex_t degree;
        stream >> degree;
        processed_edges += degree;

        UndirectedGraph::vertex_t v;
        for(UndirectedGraph::vertex_t i=0; i < degree; i++)
        {
            stream >> v;
            edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
        }
    }

    offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

    edges.close();
    offsets.close();
}

void bin2graph(const std::string &graph_basename, const std::string &output)
{
    UndirectedGraph G(graph_basename);
    std::ofstream out(output, std::ofstream::trunc);

    out << G.number_of_vertices() << " " << G.number_of_edges() << std::endl;

    for(UndirectedGraph::vertex_t u=0; u<G.number_of_vertices(); u++)
    {
        UndirectedGraph::vertex_t degree = G.degree(u);
        out << degree;

        const UndirectedGraph::vertex_t *neighbors = G.neighbors(u);
        for(UndirectedGraph::vertex_t d=0; d<degree; d++)
            out << " " << neighbors[d];
        out << std::endl;
    }

    out.close();
}

int main(const int argc, const char** argv)
{
    std::string output;
    std::string input;
    bool dump;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Print help and exit")
            ("dump", po::bool_switch(&dump)->default_value(false), "Dumps the contents of the given binary graph in text format")
            ("input,i", po::value<std::string>(&input)->required(), "Input graph file or basename if --dump is specified")
            ("output,o", po::value<std::string>(&output)->required(), "Output basename or file if --dump is specified");

    po::variables_map vm;

    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

        if(vm.count("help"))
        {
            std::cout << "motivo-graph2bin [OPTION]..." << std::endl;
            std::cout << "  Converts a ascii representation of a graph to Motivo's binary format or vice-versa" << std::endl << std::endl;
            std::cout << desc << std::endl;

            return EXIT_SUCCESS;
        }

        po::notify(vm);

        if(dump)
            bin2graph(input, output);
        else
            graph2bin(input, output);
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
