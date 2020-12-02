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
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "config.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/OptionsParser.h"

enum TEXT_GRAPH_FORMAT
{
	LOE, // each line is in the form "U V"
	NODE, // each line is in the form "U V1 V2 V3 ...", lines and edges in arbitrary order
	NODE_DEGREE // line U is in the form "d(U) V1 V2 V3 ..."
} ;

void graph2bin(const std::string &graph_filename, const std::string &output_basename, TEXT_GRAPH_FORMAT format = NODE_DEGREE)
{
	assert(format == NODE || format == LOE || format == NODE_DEGREE);

	UndirectedGraph::vertex_t num_verts;
	uint32_t num_edges;

	std::ifstream stream(graph_filename);
	if (!stream.is_open())
		throw std::runtime_error("Could not open file " + graph_filename);

	std::ofstream offsets(output_basename + ".gof", std::ofstream::binary | std::ofstream::trunc);
	std::ofstream edges(output_basename + ".ged", std::ofstream::binary | std::ofstream::trunc);

	stream >> num_verts >> num_edges;
	offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
	offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(UndirectedGraph::vertex_t));

	UndirectedGraph::vertex_t processed_edges = 0;

	if(format==NODE || format==LOE)
	{
		auto adj = new std::vector<UndirectedGraph::vertex_t>[num_verts];

		// Read edges
		UndirectedGraph::vertex_t u;
		UndirectedGraph::vertex_t v;
		if(format==NODE)
		{
			while(true)
			{
				stream >> u;
				if(u >= num_verts)
					throw std::runtime_error("Invalid vertex");

				std::string s;
				std::getline(stream, s);
				std::stringstream ss(s);

				if(stream.eof())
					break;
				else if(stream.fail())
					throw std::runtime_error("Unexpected error while reading input");

				while (ss >> v)
				{
					if(v==u || v >= num_verts)
						throw std::runtime_error("Invalid vertex");

					adj[u].push_back(v);
				}
			}
		}
		else //format==LOE
		{
			while(true)
			{
				if(!(stream >> u))
					break;

				if(u >= num_verts)
					throw std::runtime_error("Invalid vertex");

				if(!(stream >> v))
					throw std::runtime_error("Unexpected input termination");

				if(v==u || v >= num_verts)
					throw std::runtime_error("Invalid vertex");

				adj[u].push_back(v);
			}
		}

		// Write edges
		processed_edges = 0;
		for(u = 0; u < num_verts; u++)
		{
			offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));
			processed_edges += static_cast<UndirectedGraph::vertex_t>(adj[u].size());
			std::sort(adj[u].begin(), adj[u].end());
			for(UndirectedGraph::vertex_t i=0; i<adj[u].size(); i++)
			{
				if(i>0 && v == adj[u][i])
					throw std::runtime_error("Duplicated edge");

                v=adj[u][i];
				edges.write(reinterpret_cast<const char *>(&v), sizeof(UndirectedGraph::vertex_t));
			}
		}
	}
	else //format==NODE_DEGREE
	{
		for (UndirectedGraph::vertex_t u = 0; u < num_verts; u++)
		{
			offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

			UndirectedGraph::vertex_t degree;
			if(!(stream >> degree))
				throw std::runtime_error("Unexpected input termination");
			processed_edges += degree;

			UndirectedGraph::vertex_t v;
			for (UndirectedGraph::vertex_t i = 0; i < degree; i++)
			{
				if(!(stream >> v))
					throw std::runtime_error("Unexpected input termination");

				if(v==u || v >= num_verts)
					throw std::runtime_error("Invalid vertex");

				edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
			}
		}
	}

	offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

	if (processed_edges != 2 * num_edges)
		throw std::runtime_error("Number of edges in header does not match half the sum of degrees");

	edges.close();
	offsets.close();
}

void bin2graph(const std::string &graph_basename, const std::string &output, TEXT_GRAPH_FORMAT format = NODE_DEGREE)
{
	UndirectedGraph G(graph_basename);
	std::ofstream out(output, std::ofstream::trunc);
	out << G.number_of_vertices() << " " << G.number_of_edges() << "\n";
	switch(format)
    {
        case LOE:
        {
            for (UndirectedGraph::vertex_t u = 0; u < G.number_of_vertices(); u++)
            {
                const UndirectedGraph::vertex_t degree = G.degree(u);
                for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
					out << u << " " << G.neighbor(u, d) << "\n";
            }
            break;
        }
		case NODE_DEGREE: [[fallthrough]];
		case NODE:
		{
			for (UndirectedGraph::vertex_t u = 0; u < G.number_of_vertices(); u++)
			{
				const UndirectedGraph::vertex_t degree = G.degree(u);
				out << ((format==NODE)?u:degree);
				for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
					out << " " << G.neighbor(u, d);
				out << "\n";
			}
			break;
		}
		default:
			throw std::runtime_error("Invalid format"); //Should never happen

	}
	out.close();
}

int main(const int argc, const char** argv)
{
	std::cout << "This is motivo-graph. Version: " << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

	OptionsParser op;
	OptionsParser::Option* help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
	OptionsParser::Option* dump_opt = op.add_option(false, false, "dump", '\0', "", "Dumps the contents of the given binary graph in text format");
	OptionsParser::Option* input_opt = op.add_option(true, true, "input", 'i', "", "Input graph file or basename if --dump is specified (required)");
	OptionsParser::Option* output_opt = op.add_option(true, true, "output", 'o', "", "Output basename or file if --dump is specified (required)");
	OptionsParser::Option* fmt_opt = op.add_option(false, true, "format", 'f', "NODE_DEGREE", "Text format: LOE (list of edges, one edge per line), NODE (one node and its neighbors per line), NODE_DEGREE (the i-th line contains the degree of node i followed by its neighbors, default)");

	bool parse_ok = op.parse(argc, argv);
	if (!parse_ok || help_opt->is_found())
	{
		std::cout << "motivo-graph [OPTION]..." << std::endl;
		std::cout << "  Converts a ASCII representation of a graph to Motivo's binary format or vice-versa" << std::endl << std::endl;
		std::cout << op.help() << std::endl;

		return parse_ok ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (!op.has_required_options())
	{
		std::cout << "Required options are missing" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		std::string fmt = fmt_opt->get_value();
		TEXT_GRAPH_FORMAT graph_fmt;
		if (fmt == "NODE")
			graph_fmt = NODE;
		else if (fmt == "LOE")
			graph_fmt = LOE;
		else if(fmt == "NODE_DEGREE")
			graph_fmt = NODE_DEGREE;
		else
			throw std::runtime_error("Invalid graph format");

		if (dump_opt->is_found())
			bin2graph(input_opt->get_value(), output_opt->get_value(), graph_fmt);
		else
			graph2bin(input_opt->get_value(), output_opt->get_value(), graph_fmt);
	}
	catch(std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
