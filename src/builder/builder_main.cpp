#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>

#include "../common/graph/UndirectedGraph.h"
#include "builder.h"
#include "Size1ColorCoding.h"
#include "SequentialColorCoding.h"
#include "MultithreadedColorCoding.h"

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
                  << opts.to_vertex << " using " << opts.threads << " thread(s)" << std::endl;


        //FIXME: Progress?

        bool selective = *opts.selective_filename!='\0' && opts.size>1;
        TreeletSelector* selector = nullptr;
        if(selective)
        {
            selector = new TreeletSelector(opts.selective_filename, opts.size);
            std::cout << "Selectively " << ((selector->get_mode()==TreeletSelector::MODE_INCLUDE)?"counting only ":"ignoring ") << selector->get_size() << " treelet(s) of the given size" << std::endl;
        }

        std::chrono::time_point<std::chrono::steady_clock> tstart;
        if(opts.size==1)
        {
            Random rng(opts.seed);
            Size1ColorCoding builder(G.number_of_vertices(), opts.colors, opts.store0, &rng, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else if(opts.threads==1)
        {
            SequentialColorCoding builder(&G, opts.size, &ttc, opts.store0, selector, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else
        {
            MultithreadedColorCoding builder(&G, opts.size, &ttc, opts.store0, selector, &out, opts.threads);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

        std::cerr << "Building time: " << delta_t.count() << " s\n";

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        // write info for later phases
        std::ofstream infofile;
        infofile.open(std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".info", std::ofstream::trunc);
        infofile << "StoreOnlyOn0 " << std::to_string(opts.store0) << std::endl;
        infofile.close();

        delete selector;

        for(unsigned int i=0; i<opts.size-1; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
