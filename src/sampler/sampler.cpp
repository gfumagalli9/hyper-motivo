//
// Created by steven on 12/3/16.
//

#include <new>
#include <fstream>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "../common/OptionsParser.h"

void sample(const UndirectedGraph &G, const TreeletTableCollection &ttc, unsigned int size, uint64_t num_samples,
            std::ostream& out, bool text, bool canonicize, bool graphlets, bool footprints_only, Random* rng)
{
    TreeletSampler sampler(&G, &ttc, rng);
    UndirectedGraph::vertex_t occ_vertices[16] = {0};

    //FIXME: Cache spanning trees count and/or footprints?


    uint64_t sampled=0;
    uint64_t rejected=0;

    Occurrence occurrence;
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

        if(graphlets)
        {
            new (&occurrence) Occurrence(size, occ_vertices, &G);
            if(rng->random_uint<uint64_t>(0, occurrence.number_of_spanning_trees())!=0)
            {
                rejected++;
                continue; //Rejection
            }
        }
        else
            new (&occurrence) Occurrence(occ_vertices, t);

        if(canonicize)
            occurrence.canonicize();

        if(text)
            out << (footprints_only? occurrence.text_footprint():occurrence.to_string()) << std::endl;
        else
        {
            out.write(occurrence.binary_footprint(), Occurrence::binary_footprint_bytes);

            if(!footprints_only)
                out.write(reinterpret_cast<const char*>(occurrence.vertices()), static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t)*occurrence.size));
        }

        sampled++;
    }

    std::cerr << "Sampled: " << sampled << " Rejected:" << rejected << std::endl;
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
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator (default or empty string: seed from system random device)");

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

        Random rng(seed_opt->get_value());

        sample(G, ttc, static_cast<unsigned int>(size), static_cast<uint64_t>(num_samples), outfile, text_opt->is_found(), canonicize_opt->is_found(), graphlets_opt->is_found(), footprints_opt->is_found(), &rng);
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}