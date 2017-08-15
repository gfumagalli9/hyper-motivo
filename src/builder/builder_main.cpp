#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>

#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"
#include "StaticSequencer.h"
#include "builder.h"

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build. Version: " << MOTIVO_VERSION_STRING << std::endl;

    static builder_opts opts;
    try
    {
        if(!parse_builder_args(argc, argv, "motivo-build", &opts))
            return EXIT_SUCCESS;

        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        std::cout << "Using a thread batch size of " << opts.batch_size << std::endl;

        std::unique_ptr<GraphColoring> coloring;
        if(opts.size == 1)
        {
            std::cout << "Generating random coloring of " << (opts.to_vertex-opts.from_vertex+1) << " vertices using " << std::to_string(opts.colors) << " colors" << std::endl;
            Random rng(opts.seed);
            std::cout << "Using seed: \"" << rng.get_seed() <<"\"" << std::endl;
            coloring = std::make_unique<GraphColoring>(opts.from_vertex, opts.to_vertex, opts.colors, &rng);
        }

        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>* readers = nullptr;
        TreeletTable** tables = nullptr;
        if(opts.size != 1)
        {
            std::cout << "Loading tables for smaller sizes" << std::endl;
            readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>[opts.size-1];
            tables = new TreeletTable*[opts.size-1];

            for(unsigned int i=0; i<opts.size-1; i++)
            {
                readers[i].open( std::string(opts.tables_basename) + "." + std::to_string(i+1) + ".dtz" );
                readers[i].prefault(opts.from_vertex, opts.to_vertex);
                tables[i] = new TreeletTable(&readers[i]);
                ttc.add(tables[i]);
            }
        }

        const std::string filename = std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".cnt";
        std::ofstream out(filename , std::ofstream::binary | std::ofstream::trunc);
        if(out.bad())
            throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing counts of treelets of size " << opts.size << " for vertices " << opts.from_vertex << "--"
                  << opts.to_vertex << " using " << opts.threads << " worker thread(s)" << std::endl;

        StaticSequencer sequencer(opts.from_vertex, opts.to_vertex, opts.batch_size);
        if(opts.progress > 0)
        {
            std::cout << "Will print a progress report every " << opts.progress << " processed vertices" << std::endl;
            sequencer.set_progress_callback( [](UndirectedGraph::vertex_t next) -> void { report_progress(next, opts.from_vertex, opts.to_vertex); }, opts.progress);
        }

        TreeletTableBuilder builder(&G, coloring.get(), opts.size, &ttc, &out, &sequencer, opts.store0, opts.threads);
        builder.build();

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        for(unsigned int i=0; i<opts.size-1; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
