//
// Created by steven on 12/3/16.
//

#include <fstream>
#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "sampler_opts.h"
#include "sampler_impl.h"


int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

    sampler_opts opts;
    try
    {
        parse_sampler_args(argc, argv, "motivo-sample", &opts);

        std::ostream* output = &std::cout;
        if(strlen(opts.output_basename)!=0)
            output = new std::ofstream(std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".samples", std::ofstream::binary | std::ofstream::trunc);

        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        std::cerr << "Loading tables and root sampler" << std::endl;
        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>* readers = nullptr;
        TreeletTable** tables = nullptr;
        std::cout << "Loading tables for smaller sizes" << std::endl;
        readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>[opts.size];
        tables = new TreeletTable*[opts.size];

        for(unsigned int i=0; i<opts.size; i++)
        {
            readers[i].open( std::string(opts.tables_basename) + "." + std::to_string(i+1) + ".dtz" );
            readers[i].prefault(0, G.number_of_vertices()-1);
            tables[i] = new TreeletTable(&readers[i]);
            ttc.add(tables[i]);
        }
        tables[opts.size-1]->load_root_sampler(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".rts" );

        Random rng(opts.seed);
        std::cerr << "Using seed " << rng.get_seed() << std::endl;
        std::cerr << "Sampling..." << std::endl;

        sample(G, ttc, opts.size, opts.number_of_samples, opts.number_of_accepted_samples, *output, opts.text,
               opts.canonicize, opts.graphlets, opts.norejection, opts.footprints, opts.spanning_trees, opts.vertices, &rng);

        for(unsigned int i=0; i<opts.size-1; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;

        if(strlen(opts.output_basename)!=0)
            delete output;

    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}