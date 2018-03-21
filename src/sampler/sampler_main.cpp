//
// Created by steven on 12/3/16.
//

#include <fstream>
#include "../common/graph/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "sampler_opts.h"
#include "OccurrenceSampler.h"


int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

    sampler_opts opts;
    try
    {
        if(!parse_sampler_args(argc, argv, "motivo-sample", &opts))
            return EXIT_SUCCESS;

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

        TreeletSelector* selector = nullptr;
        if(*opts.selective_filename!='\0')
        {
            selector = new TreeletSelector(opts.selective_filename, opts.size);
            std::cout << "Selectively " << ((selector->get_mode()==TreeletSelector::MODE_INCLUDE)?"sampling only ":"ignoring ") << selector->get_size() << " treelet(s) of the given size" << std::endl;
        }

        std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;

        OccurrenceSampler sampler(&G, &ttc, opts.size, opts.number_of_samples, &rng, opts.vertices, opts.graphlets,
                                  opts.spanning_trees, opts.footprints, opts.canonicize, opts.norejection, opts.text, opts.group,
                                  output, opts.threads, selector);

        std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();
        sampler.sample();
        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
        std::cerr << "Sampling time: " << delta_t.count() << " s\n";

        delete selector;

        for(unsigned int i=0; i<opts.size; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;

        if(strlen(opts.output_basename)!=0)
            delete output;
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}