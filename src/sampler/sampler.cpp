//
// Created by steven on 12/3/16.
//

#include <new>
#include <fstream>
#include <chrono>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "../common/OptionsParser.h"

void sample [[gnu::hot]] (const UndirectedGraph &G, const TreeletTableCollection &ttc, const unsigned int size, const uint64_t num_samples,
            const uint64_t num_accepted, std::ostream& out, const  bool text, const bool canonicize, const  bool graphlets,
            const bool no_rejection, const bool footprints, const bool spanning_trees_no, const  bool vertices, Random* rng)
{
    TreeletSampler sampler(&G, &ttc, rng);
    UndirectedGraph::vertex_t sampled_vertices[16] = {0};

    //FIXME: Cache spanning trees count and/or footprints?

    std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();

    uint64_t sampled=0;
    uint64_t accepted=0;

    Occurrence occurrence;
    uint64_t spanning_trees = 1;
    while(sampled<num_samples && accepted<num_accepted)
    {
        sampled++;
        UndirectedGraph::vertex_t root = sampler.sample_root(size);
        assert(root<G.number_of_vertices());
        Treelet t = sampler.sample_treelet(size, root);

        if(vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
        {
#ifndef NDEBUG
            bool success =
#endif
            sampler.sample_rooted_occurrence(t, root, sampled_vertices);
            assert(success);
        }

        if(graphlets)
        {
            new (&occurrence) Occurrence(size, &G, sampled_vertices);

            if(!no_rejection || spanning_trees_no)
                spanning_trees = occurrence.number_of_spanning_trees();

            if(!no_rejection && rng->random_uint<uint64_t>(0, spanning_trees-1)!=0)
                continue; //Rejection
        }
        else
            new (&occurrence) Occurrence(t, sampled_vertices);

        if(canonicize)
            occurrence.canonicize();

        if(text)
        {
            if(footprints)
                out << occurrence.text_footprint() << ";";

            if(spanning_trees_no)
                out << spanning_trees << ";";

            if(vertices)
            {
                const UndirectedGraph::vertex_t* verts = occurrence.vertices();
                for(unsigned int i=0; i<size; i++)
                    out << verts[i] << ((i==size-1)?";":" ");
            }

            out << "\n";
        }
        else
        {
            if(footprints)
                out.write(occurrence.binary_footprint(), Occurrence::binary_footprint_bytes);

            if(spanning_trees_no)
                out.write(reinterpret_cast<const char*>(&spanning_trees), sizeof(uint64_t));

            if(vertices)
                out.write(reinterpret_cast<const char*>(occurrence.vertices()), static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t)*occurrence.size));
        }

        accepted++;
    }


    std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

    std::cerr << "Sampling time: " << delta_t.count() << "\n";
    std::cerr << "Sampled treelets: " << sampled << " (" << static_cast<double>(sampled)/delta_t.count() << " occ/s)" << "\n";
    std::cerr << "Accepted treelets/graphlets: " << accepted<< " (" << static_cast<double>(accepted)/delta_t.count() << " occ/s)" << "\n";
    std::cerr << "Rejected treelets/graphlets: " << sampled - accepted << std::endl;
}

int main(const int argc, const char** argv)
{

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the treelets to sample (required)");
    OptionsParser::Option *numsamples_opt = op.add_option(false, true, "num-samples", 'n', "", "Stop after this number of samples (default: unlimited)");
    OptionsParser::Option *numaccepted_opt = op.add_option(false, true, "num-accepted", 'a', "", "Stop after this number of accepted samples (default: unlimited)");
    OptionsParser::Option *input_opt = op.add_option(true, true, "input", 'i', "", "Input tables basename (required)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");
    OptionsParser::Option *text_opt = op.add_option(false, false, "text", 't', "", "Output occurrences in text format");
    OptionsParser::Option *canonicize_opt = op.add_option(false, false, "canonicize", 'c', "", "Output occurrences in canonical format");
    OptionsParser::Option *graphlets_opt = op.add_option(false, false, "graphlets", '\0', "", "Sample graphlets occurrences (instead of treelets)");
    OptionsParser::Option *norejection_opt = op.add_option(false, false, "no-rejection", '\0', "", "Do not perform rejection on the sampled graphlets");
    OptionsParser::Option *footprints_opt = op.add_option(false, false, "footprints", '\0', "", "Output the graphlet/treelet footprints");
    OptionsParser::Option *spanning_opt = op.add_option(false, false, "spanning-trees-no", '\0', "", "Output the number of spanning trees in the sampels treelet/graphlet");
    OptionsParser::Option *vertices_opt = op.add_option(false, false, "vertices", '\0', "", "Output the IDs of the sampled vertices");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator (default or empty string: seed from system random device)");

    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-sample [OPTION]..." << std::endl;
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

        if(!numsamples_opt->is_found() && !numsamples_opt->is_found())
            throw std::runtime_error("At least one of 'num-samples' and 'num-accepted' must be specified");

        uint64_t num_samples = std::numeric_limits<uint64_t>::max();
        if(numsamples_opt->is_found())
            num_samples = std::stoull(numsamples_opt->get_value());
        if(num_samples==0)
            throw std::runtime_error("'num-samples' option is invalid");

        uint64_t num_accepted = std::numeric_limits<uint64_t>::max();
        if(numaccepted_opt->is_found())
            num_accepted = std::stoull(numaccepted_opt->get_value());
        if(num_accepted==0)
            throw std::runtime_error("'num-accepted' option is invalid");

        if(!footprints_opt->is_found() && !spanning_opt->is_found() && !vertices_opt->is_found())
            throw std::runtime_error("Nothing to output. Please specify at least one of --footprints, --spanning-trees-no, --vertices");

        std::ofstream outfile(output_opt->get_value(), std::ofstream::binary | std::ofstream::trunc);
        UndirectedGraph G(graph_opt->get_value());
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;
        std::cerr << "Loading tables and root sampler" << std::endl;
        TreeletTableCollection ttc(input_opt->get_value(), static_cast<unsigned int>(size));
        ttc.load_root_sampler_for(input_opt->get_value(), static_cast<unsigned int>(size));

        Random rng(seed_opt->get_value());

        std::cerr << "Sampling..." << std::endl;

        sample(G, ttc, static_cast<unsigned int>(size), num_samples, num_accepted, outfile, text_opt->is_found(),
               canonicize_opt->is_found(), graphlets_opt->is_found(), norejection_opt->is_found(), footprints_opt->is_found(),
               spanning_opt->is_found(), vertices_opt->is_found(), &rng);
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}