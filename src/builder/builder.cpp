#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>

#include "../common/UndirectedGraph.h"
#include "TreeletTableBuilder.h"
#include "../common/OptionsParser.h"
#include "StaticSequencer.h"
#include "builder_opts.h"

void progress_callback(UndirectedGraph::vertex_t);



builder_opts opts;

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build. Version: " << MOTIVO_VERSION_STRING << std::endl;

    try
    {
        if(!parse_builder_args(argc, argv, "motivo-build", &opts))
            return EXIT_SUCCESS;

        UndirectedGraph G(opts.graph);
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
            sequencer.set_progress_callback(progress_callback, opts.progress);
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

typedef std::chrono::time_point<std::chrono::steady_clock> tp;

double timing(const std::string& name, const tp tstart, const tp tend, const UndirectedGraph::vertex_t vstart, const  UndirectedGraph::vertex_t vend)
{
    UndirectedGraph::vertex_t delta_v = vend-vstart;
    std::chrono::duration<double> delta_t = tend - tstart;

    double speed = delta_v/delta_t.count();

    std::cout << "Timing (" << name << "): Processed " << delta_v << " vertices in " << delta_t.count() << " seconds (" << speed << "v/s)"<< std::endl;

    return speed;
}

void progress_callback(UndirectedGraph::vertex_t next)
{
    static UndirectedGraph::vertex_t timing_no = 0;
    static UndirectedGraph::vertex_t previous_progress;
    static tp start_time;
    static tp previous_time;
    static double speed_mean; //Geometric mean of speed

    tp current_time = std::chrono::steady_clock::now();

    if(timing_no==0)
    {
        previous_progress = next;
        start_time = current_time;
        previous_time = current_time;
        timing_no++;
        return;
    }

    if(next>=previous_progress && current_time >= previous_time + std::chrono::seconds(10)) //No guarantee on the order of the callbacks
    {
        UndirectedGraph::vertex_t processed = next-opts.from_vertex;
        static UndirectedGraph::vertex_t total = opts.to_vertex-opts.from_vertex+1;

        std::cout << "Progress report #"<< timing_no <<": " << (100*static_cast<double>(processed)/total) << "%" << std::endl;
        timing("all", start_time, current_time, 0, processed);
        double speed = timing("last", previous_time, current_time, previous_progress, next);

        if(timing_no==1)
            speed_mean = speed;
        else
            speed_mean = 0.2 * speed + 0.8 * speed_mean;

        long ttc = static_cast<long>((total-processed)/speed_mean);
        int ttc_s = static_cast<int>(ttc%60);
        ttc/=60;
        int ttc_m = static_cast<int>(ttc%60);
        ttc/=60;
        int ttc_h = static_cast<int>(ttc%24);
        ttc /= 24;

        std::cout << "ETC: " << ttc << " days " << ttc_h << "h " << ttc_m << "m " << ttc_s <<"s" << std::endl;
        std::cout << std::flush;

        previous_progress = next;
        previous_time = current_time;
        timing_no++;
    }
}