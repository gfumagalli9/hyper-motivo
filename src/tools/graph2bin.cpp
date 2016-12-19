//
// Created by steven on 12/3/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "../common/UndirectedGraph.h"

namespace po = boost::program_options;

int main(const int argc, const char** argv)
{
    std::string output_basename;
    std::string graph_filename;

    po::options_description visible_desc("Allowed options");
    visible_desc.add_options()
            ("help", "Print help and exit")
            ("output,o", po::value<std::string>(&output_basename)->required(), "Output basename (required)");

    po::options_description hidden_desc("Hidden options");
    hidden_desc.add_options()("input", po::value<std::string>(&graph_filename), "Input graph file");

    po::positional_options_description positional_desc;
    positional_desc.add("input", 1);

    po::options_description desc;
    desc.add(visible_desc).add(hidden_desc);

    po::variables_map vm;

    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_desc).run(), vm);

        if (vm.count("help"))
        {
            std::cout << "motivo-graph2bin --output BASENAME INPUT_GRAPH" << std::endl;
            std::cout << "  Converts a ascii representation of a graph to Motivo's binary format" << std::endl << std::endl;
            std::cout << visible_desc << std::endl;

            return EXIT_SUCCESS;
        }

        po::notify(vm);

        UndirectedGraph::vertex_t num_verts;
        uint32_t num_edges;

        std::ifstream stream(graph_filename);

        std::ofstream offsets( output_basename + ".gof", std::ofstream::binary | std::ofstream::trunc);
        std::ofstream edges( output_basename + ".ged", std::ofstream::binary | std::ofstream::trunc);

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
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
