//
// Created by steven on 12/3/16.
//

#include <fstream>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "../common/OptionsParser.h"

void sample(const UndirectedGraph &G, const TreeletTableCollection &ttc, unsigned int size, uint64_t num_samples,
            std::ostream& out, bool text, bool canonicize, bool graphlets, bool footprints_only)
{
    Random rng;
    TreeletSampler sampler(&G, &ttc, &rng);
    UndirectedGraph::vertex_t occ_vertices[16] = {0};

    //FIXME: Cache spanning trees count and/or footprints?

    //GraphFootprint footprint;
    uint64_t sampled=0;
    uint64_t rejected=0;
    while(sampled<num_samples)
    {
        UndirectedGraph::vertex_t root = sampler.sample_root(size);
        assert(root<G.number_of_vertices());
        Treelet t = sampler.sample_treelet(size, root);
#ifndef NDEBUG
        bool success =
#endif
        sampler.sample_rooted_occurrence(t, root, occ_vertices);
        assert(success);

        Occurrence *occurrence = nullptr;
        if(graphlets)
        {
            occurrence = new Occurrence(size, occ_vertices, &G);
            uint64_t spanning_trees=occurrence->number_of_spanning_trees();

            if(rng.random_uint64(0, spanning_trees)!=0)
            {
                rejected++;
                delete occurrence;
                continue; //Rejection
            }
        }
        else
            occurrence = new Occurrence(occ_vertices, t);

        if(canonicize)
            occurrence->canonicize();

        if(text)
            out << (footprints_only?occurrence->footprint():occurrence->to_string()) << std::endl;
        else
        {
            //FIXME
            out.write(reinterpret_cast<const char*>(occ_vertices), static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t)*size));
        }

        delete occurrence;
        sampled++;
    }

    std::cout << "Sampled: " << sampled << " Rejected:" << rejected << std::endl;
}

int main(const int argc, const char** argv)
{

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the treelets to sample (required)");
    OptionsParser::Option *samples_opt = op.add_option(false, true, "num-samples", 'n', "1", "Number of samples (default: 1)");
    OptionsParser::Option *input_opt = op.add_option(true, true, "input", 'i', "", "Input tables basename (required)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");
    OptionsParser::Option *text_opt = op.add_option(false, false, "text", 't', "", "Output occurrences in text format");
    OptionsParser::Option *canonicize_opt = op.add_option(false, false, "canonicize", 'c', "", "Output occurrences in canonical format");
    OptionsParser::Option *graphlets_opt = op.add_option(false, false, "graphlets", '\0', "", "Sample graphlets occurrences (instead of treelets)");
    OptionsParser::Option *footprints_opt = op.add_option(false, false, "footprints-only", 'f', "", "Only output the footprints (and not the actual vertices)");

    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-sample [OPTION]... BASENAME" << std::endl;
        std::cout << "  Samples treelets from tables" << std::endl << std::endl;
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

        int64_t num_samples = std::stoll(samples_opt->get_value());
        if(num_samples<=0)
            throw std::runtime_error("'num-samples' option is invalid");


        std::ofstream outfile(output_opt->get_value(), std::ofstream::binary | std::ofstream::trunc);
        UndirectedGraph G(graph_opt->get_value());
        TreeletTableCollection ttc(input_opt->get_value(), static_cast<unsigned int>(size));

        sample(G, ttc, static_cast<unsigned int>(size), static_cast<uint64_t>(num_samples), outfile, text_opt->is_found(), canonicize_opt->is_found(), graphlets_opt->is_found(), footprints_opt->is_found());
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}