#include <cstdlib>
#include <limits>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include "config.h"
#include "../common/OptionsParser.h"
#include "../common/graph/UndirectedGraph.h"
#include "Size1Builder.h"
#include "SequentialBuilder.h"
#include "MultithreadedBuilder.h"
#include "../common/io/PropertyStore.h"

struct builder_opts
{
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint8_t colors;
    char tables_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3]; //Enough space to append one character + 1 integer
    unsigned int threads;
    char output_basename[MOTIVO_ARG_MAX];
    bool store0;
    char selective_filename[MOTIVO_ARG_MAX];
};

bool parse_builder_args(const int argc, const char **argv, const std::string &name, builder_opts *opts)
{
    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *graph_opt = op.add_option(true, true, "graph", 'g', "", "Input graph basename (required)");
    OptionsParser::Option *size_opt = op.add_option(true, true, "size", 's', "", "Size of the table to build, between 1 and 16 (required)");
    OptionsParser::Option *colors_opt = op.add_option(false, true, "colors", 'c', "0", "Number of colors to use, between 1 and 16 (required if size=1, ignored if size>1)");
    OptionsParser::Option *tables_opt = op.add_option(false, true, "tables-basename", 'i', "", "Basename of table files of smaller size (required if size > 1, ignored if size=1)");
    OptionsParser::Option *from_opt = op.add_option(false, true, "from-vertex", '\0', "", "First vertex (default: 0)");
    OptionsParser::Option *to_opt = op.add_option(false, true, "to-vertex", '\0', "", "Last vertex (default: last vertex if the graph)");
    OptionsParser::Option *seed_opt = op.add_option(false, true, "seed", '\0', "", "String used to seed the random number generator for the initial coloring (default or empty string: seed from system random device)");
    OptionsParser::Option *threads_opt = op.add_option(false, true, "threads", '\0', "1", "Number of threads to use or 0 for to use the number of logical processors (default: 1, ignored if size=1)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output file (required)");
    OptionsParser::Option *store0_opt  = op.add_option(false, false, "store-on-0-colored-vertices-only", '0', "", "Store treelet counts only for the vertices with color 0 (default: false)");
    OptionsParser::Option *selective_opt = op.add_option(false, true, "selective", '\0', "", "Count only treelets whose structures are allowed in file ARG");

    if (!op.parse(argc, argv) || help_opt->is_found())
    {
        std::cout << name << " [OPTION]..." << std::endl;
        std::cout << "  Builds count tables for use with motivo-merge" << std::endl << std::endl;
        std::cout << op.help() << std::endl;
        return false;
    }

    if(!op.has_required_options())
        throw std::runtime_error("Required options are missing");

    int size = std::stoi(size_opt->get_value());
    if (size < 1 || size > 16)
        throw std::runtime_error("'size' option is invalid");
    opts->size = static_cast<unsigned int>(size);

    int colors = std::stoi(colors_opt->get_value());
    if(size==1 && (!colors_opt->is_found() || colors < 1 || colors > 16))
        throw std::runtime_error("'colors' option missing or invalid");
    opts->colors = static_cast<uint8_t>(colors);

    if(size != 1 && !tables_opt->is_found())
        throw std::runtime_error("'tables-basename' option is required");
    if(tables_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'tables-basename' option is too long");
    strcpy(opts->tables_basename,tables_opt->get_value().c_str());

    if(graph_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'graph' option is too long");
    strcpy(opts->graph,graph_opt->get_value().c_str());

    UndirectedGraph G(graph_opt->get_value());

    opts->from_vertex = 0;
    if(from_opt->is_found())
    {
        int64_t from = std::stoll(from_opt->get_value());
        if(from < 0 || from >=G.number_of_vertices())
            throw std::runtime_error("'from-vertex' option specifies an invalid vertex");

        opts->from_vertex = static_cast<UndirectedGraph::vertex_t>(from);
    }

    opts->to_vertex = G.number_of_vertices()-1;
    if(to_opt->is_found())
    {
        int64_t to = std::stoll(to_opt->get_value());
        if(to < 0 || to >=G.number_of_vertices())
            throw std::runtime_error("'to-vertex' option specifies an invalid vertex");

        opts->to_vertex = static_cast<UndirectedGraph::vertex_t>(to);
    }

    if(opts->from_vertex>opts->to_vertex)
        throw std::runtime_error("'from-vertex' and 'to-vertex' options specify an empty range");

    if(opts->size==1)
        opts->threads=1;
    else
    {
        int threads = std::stoi(threads_opt->get_value());
        if (threads < 0)
            throw std::runtime_error("The number of threads is invalid");

        if (threads == 0)
            opts->threads = std::thread::hardware_concurrency();
        else
            opts->threads = static_cast<unsigned int>(threads);

        if (opts->threads <= 0)
            throw std::runtime_error("Failed to determine the number of logical processors");
    }

    if(output_opt->get_value().size()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    strcpy(opts->output_basename, output_opt->get_value().c_str());

    opts->store0 = store0_opt->is_found();

    if(seed_opt->get_value().length()>=MOTIVO_ARG_MAX)
        throw std::runtime_error("'seed' option is too long");
    strcpy(opts->seed, seed_opt->get_value().c_str());

    if(selective_opt->is_found())
    {
        if(selective_opt->get_value().length()>=MOTIVO_ARG_MAX)
            throw std::runtime_error("'selective' option is too long");
        strcpy(opts->selective_filename, selective_opt->get_value().c_str());
    }
    else
        *(opts->selective_filename)='\0';

    return true;
}

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

        bool selective = *opts.selective_filename!='\0' && opts.size>1;
        TreeletStructureSelector* selector = nullptr;
        if(selective)
        {
            selector = new TreeletStructureSelector(TreeletStructureSelector(opts.selective_filename).restrict_to_sizes(opts.size,opts.size));
            std::cout << "Selectively " << ((selector->get_mode()==TreeletStructureSelector::MODE_INCLUDE)?"counting only ":"ignoring ") << selector->size() << " treelet(s) of the given size" << std::endl;
        }

        std::chrono::time_point<std::chrono::steady_clock> tstart;
        if(opts.size==1)
        {
            Random rng(opts.seed);
            Size1Builder builder(G.number_of_vertices(), opts.from_vertex, opts.to_vertex, opts.colors, opts.store0, &rng, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else if(opts.threads==1)
        {
            SequentialBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size, &ttc, opts.store0, selector, &out);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        else
        {
            MultithreadedBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size, &ttc, opts.store0, selector, &out, opts.threads);
            tstart = std::chrono::steady_clock::now();
            builder.build();
        }
        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

        std::cerr << "Building time: " << delta_t.count() << " s\n";

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        // write info for later phases
        PropertyStore properties;
        properties.set_bool("StoreOnlyOn0", opts.store0);
        properties.save(std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".info");

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
