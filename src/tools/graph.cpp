//
// Created by steven on 12/3/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <config.h>
#include "../common/graph/UndirectedGraph.h"
#include "../common/OptionsParser.h"

void graph2bin(const std::string &graph_filename, const std::string &output_basename)
{
    UndirectedGraph::vertex_t num_verts;
    uint32_t num_edges;

    std::ifstream stream(graph_filename);
    if(!stream.is_open())
        throw std::runtime_error("Could not open file " + graph_filename);

    std::ofstream offsets(output_basename + ".gof", std::ofstream::binary | std::ofstream::trunc);
    std::ofstream edges(output_basename + ".ged", std::ofstream::binary | std::ofstream::trunc);

    stream >> num_verts >> num_edges;
    offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
    offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(UndirectedGraph::vertex_t));

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
            assert(v < num_verts);
            edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
        }
    }

    offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

    if(processed_edges != 2*num_edges)
        throw std::runtime_error("Number of edges in header does not match half the sum of degrees");

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

        for(UndirectedGraph::vertex_t d=0; d<degree; d++)
        {
            const UndirectedGraph::vertex_t v = G.neighbor(u, d);
            assert(v < G.number_of_vertices());
            out << " " << v;
        }
        out << std::endl;
    }

    out.close();
}

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-graph. Version: " << MOTIVO_VERSION_STRING << std::endl;

    OptionsParser op;
    OptionsParser::Option* help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option* dump_opt = op.add_option(false, false, "dump", '\0', "", "Dumps the contents of the given binary graph in text format");
    OptionsParser::Option* input_opt = op.add_option(true, true, "input", 'i', "", "Input graph file or basename if --dump is specified (required)");
    OptionsParser::Option* output_opt = op.add_option(true, true, "output", 'o', "", "Output basename or file if --dump is specified (required)");

    bool parse_ok = op.parse(argc, argv);
    if(!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-graph [OPTION]..." << std::endl;
        std::cout << "  Converts a ascii representation of a graph to Motivo's binary format or vice-versa" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return parse_ok?EXIT_SUCCESS:EXIT_FAILURE;
    }

    if(!op.has_required_options())
    {
        std::cout << "Required options are missing" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        if(dump_opt->is_found())
            bin2graph(input_opt->get_value(), output_opt->get_value());
        else
            graph2bin(input_opt->get_value(), output_opt->get_value());
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
