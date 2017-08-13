//
// Created by steven on 12/3/16.
//

#include <new>
#include <fstream>
#include <chrono>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "sampler_opts.h"

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

    sampler_opts opts;
    try
    {
        parse_sampler_args(argc, argv, "motivo-sample", &opts);

        std::ostream* output = &std::cout;
        if(strlen(opts.output)!=0)
            output = new std::ofstream(opts.output, std::ofstream::binary | std::ofstream::trunc);

        UndirectedGraph G(opts.graph);
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;
        std::cerr << "Loading tables and root sampler" << std::endl;

        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>* readers = nullptr;
        TreeletTable** tables = nullptr;
        std::cout << "Loading tables for smaller sizes" << std::endl;
        readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>[opts.size-1];
        tables = new TreeletTable*[opts.size-1];

        for(unsigned int i=0; i<opts.size-1; i++)
        {
            readers[i].open( std::string(opts.tables_basename) + "." + std::to_string(i+1) + ".dtz" );
            readers[i].prefault(0, G.number_of_vertices()-1);
            tables[i] = new TreeletTable(&readers[i]);
            ttc.add(tables[i]);
        }

        Random rng(opts.seed);

        std::cerr << "Sampling..." << std::endl;

        sample(G, ttc, opts.size, opts.number_of_samples, opts.number_of_accepted_samples, *output, opts.text,
               opts.canonicize, opts.graphlets, opts.norejection, opts.norejection, opts.spanning_trees, opts.vertices, &rng);

        for(unsigned int i=0; i<opts.size-1; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;

        if(strlen(opts.output)!=0)
            delete output;

    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}