//
// Created by steven on 12/3/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <config.h>
#include "../common/graph/UndirectedGraph.h"
#include "../common/OptionsParser.h"

enum TEXT_GRAPH_FORMAT
{
	ARC, // each line is in the form "U V"
	NODE, // each line is in the form "U V1 V2 V3 ...", lines and edges in arbitrary order
	NODE_DEGREE // line U is in the form "d(U) V1 V2 V3 ..."
} ;

void graph2bin(const std::string &graph_filename, const std::string &output_basename, TEXT_GRAPH_FORMAT fm = NODE_DEGREE)
{
	UndirectedGraph::vertex_t num_verts;
	uint32_t num_edges;

	std::ifstream stream(graph_filename);
	if (!stream.is_open())
		throw std::runtime_error("Could not open file " + graph_filename);

	std::ofstream offsets(output_basename + ".gof", std::ofstream::binary | std::ofstream::trunc);
	std::ofstream edges(output_basename + ".ged", std::ofstream::binary | std::ofstream::trunc);

	UndirectedGraph::vertex_t processed_edges = 0;
	switch (fm)
    {
        case NODE:
        {
            stream >> num_verts >> num_edges;
            offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
            offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(UndirectedGraph::vertex_t));
            auto adj = new std::vector<UndirectedGraph::vertex_t>[num_verts];
            // Read edges
            while (!stream.eof()) {
                UndirectedGraph::vertex_t u;
                stream >> u;
                assert(u < num_verts);
                std::string s;
                std::getline(stream, s);
                std::stringstream ss(s);
                UndirectedGraph::vertex_t v;
                while (ss >> v) {
                    assert(v < num_verts);
                    adj[u].push_back(v);
                }
            }
            // Write edges
            processed_edges = 0;
            for (UndirectedGraph::vertex_t u = 0; u < num_verts; u++) {
                offsets.write(reinterpret_cast<const char*>(&processed_edges),
                        sizeof(UndirectedGraph::vertex_t));
                for (auto v : adj[u]) {
                    edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
                }
                processed_edges += static_cast<UndirectedGraph::vertex_t>(adj[u].size());
            }
            break;
        }
		case ARC: //FIXME: ???
			break;
		default:
        {
			stream >> num_verts >> num_edges;
			offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
			offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(UndirectedGraph::vertex_t));
			for (UndirectedGraph::vertex_t u = 0; u < num_verts; u++) {
				offsets.write(reinterpret_cast<const char*>(&processed_edges),
						sizeof(UndirectedGraph::vertex_t));

				UndirectedGraph::vertex_t degree;
				stream >> degree;
				processed_edges += degree;

				UndirectedGraph::vertex_t v;
				for (UndirectedGraph::vertex_t i = 0; i < degree; i++) {
					stream >> v;
					assert(v < num_verts);
					edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
				}
			}
			break;
		}
	}
	offsets.write(reinterpret_cast<const char*>(&processed_edges),
			sizeof(UndirectedGraph::vertex_t));

	if (processed_edges != 2 * num_edges)
		throw std::runtime_error(
				"Number of edges in header does not match half the sum of degrees");

	edges.close();
	offsets.close();
}

void bin2graph(const std::string &graph_basename, const std::string &output, TEXT_GRAPH_FORMAT fm = NODE_DEGREE)
{
	UndirectedGraph G(graph_basename);
	std::ofstream out(output, std::ofstream::trunc);
	out << G.number_of_vertices() << " " << G.number_of_edges() << std::endl;
	switch (fm)
    {
        case (ARC):
        {
            for (UndirectedGraph::vertex_t u = 0; u < G.number_of_vertices(); u++)
            {
                UndirectedGraph::vertex_t degree = G.degree(u);
                std::vector<UndirectedGraph::vertex_t> neighs;
                for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
                    neighs.push_back(G.neighbor(u, d));
                std::sort(neighs.begin(), neighs.end());
                for (auto v : neighs)
                    out << u << " " << v << std::endl;
            }
            break;
        }
        default: //FIXME: What happens with NODE format??
        {
            for (UndirectedGraph::vertex_t u = 0; u < G.number_of_vertices(); u++) {
                UndirectedGraph::vertex_t degree = G.degree(u);
                out << degree;

                for (UndirectedGraph::vertex_t d = 0; d < degree; d++) {
                    const UndirectedGraph::vertex_t v = G.neighbor(u, d);
                    assert(v < G.number_of_vertices());
                    out << " " << v;
                }
                out << std::endl;
            }
            break;
        }
	}
	out.close();
}

int main(const int argc, const char** argv) {
	std::cout << "This is motivo-graph. Version: " << MOTIVO_VERSION_STRING << std::endl;

	OptionsParser op;
	OptionsParser::Option* help_opt = op.add_option(false, false, "help", '\0', "",
			"Print help and exit");
	OptionsParser::Option* dump_opt = op.add_option(false, false, "dump", '\0', "",
			"Dumps the contents of the given binary graph in text format");
	OptionsParser::Option* input_opt = op.add_option(true, true, "input", 'i', "",
			"Input graph file or basename if --dump is specified (required)");
	OptionsParser::Option* output_opt = op.add_option(true, true, "output", 'o', "",
			"Output basename or file if --dump is specified (required)");
	OptionsParser::Option* fmt_opt =
			op.add_option(false, true, "format", 'f', "",
					"Text format: ARC (one arc per line), NODE (one node and its neighbors per line), NODE_DEGREE (the i-th line contains the degree of node i followed by its neighbors)");

	bool parse_ok = op.parse(argc, argv);
	if (!parse_ok || help_opt->is_found()) {
		std::cout << "motivo-graph [OPTION]..." << std::endl;
		std::cout
				<< "  Converts a ascii representation of a graph to Motivo's binary format or vice-versa"
				<< std::endl << std::endl;
		std::cout << op.help() << std::endl;

		return parse_ok ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (!op.has_required_options()) {
		std::cout << "Required options are missing" << std::endl;
		return EXIT_FAILURE;
	}

	std::string fmt = fmt_opt->is_found() ? fmt_opt->get_value() : "NODE_DEGREE";
	TEXT_GRAPH_FORMAT graph_fmt = NODE_DEGREE;
	if (fmt == "NODE")
		graph_fmt = NODE;
	if (fmt == "ARC")
		graph_fmt = ARC;

	try {
		if (dump_opt->is_found())
			bin2graph(input_opt->get_value(), output_opt->get_value(), graph_fmt);
		else
			graph2bin(input_opt->get_value(), output_opt->get_value(), graph_fmt);
	}
	catch(std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
