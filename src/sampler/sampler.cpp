//
// Created by steven on 12/3/16.
//

#include <fstream>
#include <boost/program_options.hpp>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"

namespace po = boost::program_options;

void sample(const UndirectedGraph &G, const TreeletTableCollection &ttc, unsigned int size, uint64_t num_samples,
            std::ostream& out, bool text, bool canonicize, bool graphlets, bool footprints_only)
{
    TreeletSampler sampler(&G, &ttc);
    UndirectedGraph::vertex_t occ_vertices[16];

    //FIXME: Cache spanning trees count and/or footprints?

    //GraphFootprint footprint;
    for(uint64_t i=0; i<num_samples; i++)
    {
        UndirectedGraph::vertex_t root = sampler.sample_root(size);
        Treelet t = sampler.sample_treelet(size, root);
#ifndef NDEBUG
        bool success =
#endif
        sampler.sample_rooted_occurrence(t, root, occ_vertices);
        assert(success);

        std::unique_ptr<Occurrence> occurrence;

        if(graphlets) //FIXME: Rejection
            occurrence = std::make_unique<Occurrence>(size, occ_vertices, &G);
        else
            occurrence = std::make_unique<Occurrence>(occ_vertices, t);

        if(canonicize)
            occurrence->canonicize();

        if(text)
            out << (footprints_only?occurrence->footprint():occurrence->to_string()) << std::endl;
        else
        {
            //FIXME
            out.write(reinterpret_cast<const char*>(occ_vertices), static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t)*size));
        }
    }
}

int main(const int argc, const char** argv)
{
    std::string output_filename;
    std::string graph_basename;
    unsigned int size;
    uint64_t num_samples;
    bool text;
    bool canonicize;
    bool graphlets;
    bool footprints_only;
    std::string tables_basename;

    po::options_description visible_desc("Allowed options");
    visible_desc.add_options()
            ("help", "Print help and exit")
            ("graph,g", po::value<std::string>(&graph_basename)->required(), "Input graph basename (required)")
            ("size,s", po::value < unsigned int > (&size)->required(), "Size of the treelets to sample (required)")
            ("num-samples,n", po::value<uint64_t>(&num_samples)->default_value(1), "Number of samples (required)")
            ("output,o", po::value<std::string>(&output_filename)->required(), "Output file (required)")
            ("text,t", po::bool_switch(&text)->default_value(false), "Output occurrences in text format")
            ("canonicize,c", po::bool_switch(&canonicize)->default_value(false), "Output occurrences in canonical format")
            ("graphlets,G", po::bool_switch(&graphlets)->default_value(false), "Sample graphlets occurrences (instead of treelets)")
            ("footprints-only,f", po::bool_switch(&footprints_only)->default_value(false), "Only output the footprints (and not the actual vertices)");

    po::options_description hidden_desc("Hidden options");
    hidden_desc.add_options()("input", po::value<std::string>(&tables_basename), "Input tables basename");

    po::positional_options_description positional_desc;
    positional_desc.add("input", 1);

    po::options_description desc;
    desc.add(visible_desc).add(hidden_desc);

    po::variables_map vm;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_desc).run(), vm);
        if(vm.count("help"))
        {
            std::cout << "motivo-sample [OPTION]... BASENAME" << std::endl;
            std::cout << "  Samples treelets from tables" << std::endl << std::endl;
            std::cout << visible_desc << std::endl;

            return EXIT_SUCCESS;
        }

        po::notify(vm);

        if (size < 1 || size > 16)
            throw std::runtime_error("'size' option is invalid");

        std::ofstream outfile = std::ofstream(output_filename, std::ofstream::binary | std::ofstream::trunc);
        UndirectedGraph G(graph_basename);
        TreeletTableCollection ttc(tables_basename, size);

        sample(G, ttc, size, num_samples, outfile, text, canonicize, graphlets, footprints_only);
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}