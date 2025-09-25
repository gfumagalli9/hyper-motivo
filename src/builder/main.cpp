// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// (omissis: banner licenza)

#include <cstdlib>
#include <limits>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

#include "config.h"
#include "../common/util.h"
#include "../common/OptionsParser.h"
#include "../common/graph/UndirectedGraph.h"
#include "Size1Builder.h"
#include "SequentialBuilder.h"
#include "MultithreadedBuilder.h"
#include "../common/io/PropertyStore.h"

// NEW: pairs support (LOW∩HIGH)
#include "../common/types/PairSet.h"

struct builder_opts {
    char graph[MOTIVO_ARG_MAX];
    unsigned int size;
    uint8_t colors;
    char tables_basename[MOTIVO_ARG_MAX];
    UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    char seed[MOTIVO_ARG_MAX + 2 + std::numeric_limits<unsigned int>::digits/3];
    unsigned int threads;
    char output_basename[MOTIVO_ARG_MAX];
    bool store0;
    char selective_filename[MOTIVO_ARG_MAX];
    double coloring_bias;
    bool normalize; // NEW
};

// --- NEW: carica <graph>.pairs (se esiste) in un PairSet ---
static PairSet load_pairs_if_any(const std::string& basename) {
    const std::string filename = basename + ".pairs";
    std::ifstream in(filename, std::ios::binary);
    PairSet S;
    if (!in) return S; // assente = insieme vuoto

    std::uint64_t M = 0;
    in.read(reinterpret_cast<char*>(&M), sizeof(M));
    S.reserve(static_cast<size_t>(M * 2));
    for (std::uint64_t i = 0; i < M; ++i) {
        UndirectedGraph::vertex_t u, v;
        in.read(reinterpret_cast<char*>(&u), sizeof(u));
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        S.insert(canon_pair(u, v)); // normalizza subito (u<v)
    }
    return S;
}

static bool parse_builder_args(const int argc, const char **argv,
                               const std::string &name, builder_opts *opts)
{
    OptionsParser op;
    auto* help_opt     = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    auto* graph_opt    = op.add_option(true,  true,  "graph", 'g', "", "Input graph basename (required)");
    auto* size_opt     = op.add_option(true,  true,  "size",  's', "", "Size of the table to build, between 1 and 16 (required)");
    auto* colors_opt   = op.add_option(false, true,  "colors",'c', "0","Number of colors to use, between 1 and 16 (required if size=1, ignored if size>1)");
    auto* tables_opt   = op.add_option(false, true,  "tables-basename", 'i', "", "Basename of table files of smaller size (required if size > 1, ignored if size=1)");
    auto* from_opt     = op.add_option(false, true,  "from-vertex", '\0', "", "First vertex (default: 0)");
    auto* to_opt       = op.add_option(false, true,  "to-vertex",   '\0', "", "Last vertex (default: last vertex of the graph)");
    auto* seed_opt     = op.add_option(false, true,  "seed", '\0', "", "RNG seed for the initial coloring (default: from system RNG)");
    auto* threads_opt  = op.add_option(false, true,  "threads", '\0', "1", "Number of threads (0 = HW concurrency) (ignored if size=1)");
    auto* output_opt   = op.add_option(true,  true,  "output", 'o', "", "Output basename (required)");
    auto* store0_opt   = op.add_option(false, false, "store-on-0-colored-vertices-only", '0', "", "Store counts only for vertices with color 0 (ignored if size=1)");
    auto* selective_opt= op.add_option(false, true,  "selective", '\0', "", "Count only treelets allowed in the given file");
    auto* bias_opt     = op.add_option(false, true,  "coloring-bias", '\0', "1", "Reduce k-colorful prob by lowering first k/2 colors");
    auto* norm_opt     = op.add_option(false, true,  "normalize", '\0', "true", "Normalize counts (true/false)"); // NEW

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << name << " [OPTION]...\n"
                  << "  Builds count tables for use with motivo-merge\n\n"
                  << op.help() << std::endl;
        return false;
    }
    if (!op.has_required_options())
        throw std::runtime_error("Required options are missing");

    // Parse
    int k = std::stoi(size_opt->get_value());
    if (k < 1 || k > 16) throw std::runtime_error("'size' option is invalid");
    opts->size = static_cast<unsigned int>(k);

    int colors = std::stoi(colors_opt->get_value());
    if (k == 1 && (!colors_opt->is_found() || colors < 1 || colors > 16))
        throw std::runtime_error("'colors' option missing or invalid");
    opts->colors = static_cast<uint8_t>(colors);

    if (k != 1 && !tables_opt->is_found())
        throw std::runtime_error("'tables-basename' option is required");
    if (tables_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'tables-basename' option is too long");
    std::strcpy(opts->tables_basename, tables_opt->get_value().c_str());

    if (graph_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'graph' option is too long");
    std::strcpy(opts->graph, graph_opt->get_value().c_str());

    UndirectedGraph G(graph_opt->get_value()); // solo per validare range
    opts->from_vertex = 0;
    if (from_opt->is_found()) {
        uint64_t from = std::stoull(from_opt->get_value());
        if (from >= G.number_of_vertices())
            throw std::runtime_error("'from-vertex' is out of range");
        opts->from_vertex = static_cast<UndirectedGraph::vertex_t>(from);
    }

    opts->to_vertex = G.number_of_vertices() - 1;
    if (to_opt->is_found()) {
        uint64_t to = std::stoull(to_opt->get_value());
        if (to >= G.number_of_vertices())
            throw std::runtime_error("'to-vertex' is out of range");
        opts->to_vertex = static_cast<UndirectedGraph::vertex_t>(to);
    }
    if (opts->from_vertex > opts->to_vertex)
        throw std::runtime_error("'from-vertex'..'to-vertex' is an empty range");

    if (opts->size == 1) opts->threads = 1;
    else {
        int thr = std::stoi(threads_opt->get_value());
        if (thr < 0) throw std::runtime_error("Invalid number of threads");
        opts->threads = (thr == 0) ? std::thread::hardware_concurrency()
                                   : static_cast<unsigned int>(thr);
        if (opts->threads == 0)
            throw std::runtime_error("Failed to determine HW concurrency");
    }

    if (output_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'output' option is too long");
    std::strcpy(opts->output_basename, output_opt->get_value().c_str());

    opts->store0 = store0_opt->is_found();

    if (seed_opt->get_value().size() >= MOTIVO_ARG_MAX)
        throw std::runtime_error("'seed' option is too long");
    std::strcpy(opts->seed, seed_opt->get_value().c_str());

    if (selective_opt->is_found()) {
        if (selective_opt->get_value().size() >= MOTIVO_ARG_MAX)
            throw std::runtime_error("'selective' option is too long");
        std::strcpy(opts->selective_filename, selective_opt->get_value().c_str());
    } else {
        *(opts->selective_filename) = '\0';
    }

    opts->coloring_bias = std::stod(bias_opt->get_value());
    if (!std::isnormal(opts->coloring_bias) || opts->coloring_bias > 1 || opts->coloring_bias <= 0)
        throw std::runtime_error("'coloring-bias' must be in (0,1]");

    // NEW: normalize
    {
        const std::string v = norm_opt->get_value();
        opts->normalize = (v != "false" && v != "0" && v != "no");
    }

    return true;
}

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build. Version: " << MOTIVO_VERSION_STRING
              << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

    builder_opts opts;
    try {
        if (!parse_builder_args(argc, argv, "motivo-build", &opts))
            return EXIT_SUCCESS;

        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cout << "Loaded graph with " << G.number_of_vertices()
                  << " vertices and " << G.number_of_edges() << " edges\n";

        // Coloring distribution 
        double* color_distribution = nullptr;
        if (!double_equality(opts.coloring_bias, 1)) {
            color_distribution = new double[opts.colors];
            int lpcn = opts.colors - 1;
            double lpc = std::min(1.0 * lpcn / opts.colors, opts.coloring_bias * lpcn);
            bimodal_distribution(color_distribution, opts.colors, lpcn, lpc);
            std::cout << "color 0 has probability " << color_distribution[0] << "\n"
                      << "k-colorful probability=" << pcold(color_distribution, opts.colors) << "\n";
            if (color_distribution[0] < 100.0 / G.number_of_vertices())
                std::cerr << "Warning! Less than 100 nodes in expectation with color 0\n";
        }

        // Carico le TTC per i livelli inferiori 
        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
                                   TreeletTable::may_alias>* readers = nullptr;
        TreeletTable** tables = nullptr;
        if (opts.size != 1) {
            std::cout << "Loading tables for smaller sizes\n";
            readers = new CompressedRecordFileReader<
                const TreeletTable::treelet_count_pair_maybe_alias,
                TreeletTable::may_alias>[opts.size - 1];
            tables = new TreeletTable*[opts.size - 1];

            for (unsigned i = 0; i < opts.size - 1; ++i) {
                readers[i].open(std::string(opts.tables_basename) + "." + std::to_string(i+1) + ".dtz");
                readers[i].prefault(opts.from_vertex, opts.to_vertex);
                tables[i] = new TreeletTable(&readers[i]);
                ttc.add(tables[i]);
            }
        }

        const std::string filename = std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".cnt";
        std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
        if (!out) throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing counts of treelets of size " << opts.size
                  << " for vertices " << opts.from_vertex << "--" << opts.to_vertex
                  << " using " << opts.threads << " thread(s)\n";

        // Selettore strutture
        bool selective = *opts.selective_filename!='\0' && opts.size>1;
        TreeletStructureSelector* selector = nullptr;
        if (selective) {
            selector = new TreeletStructureSelector(
                TreeletStructureSelector(opts.selective_filename).restrict_to_sizes(opts.size, opts.size));
            std::cout << "Selectively "
                      << ((selector->get_mode()==TreeletStructureSelector::MODE_INCLUDE) ? "counting only " : "ignoring ")
                      << selector->size() << " treelet(s) of the given size\n";
        }

        // --- NEW: carica set di coppie LOW∩HIGH se presente (grafi) ---
        PairSet common_pairs;
        if (opts.size > 1) {
            try {
                common_pairs = load_pairs_if_any(opts.graph);
                if (!common_pairs.empty())
                    std::cout << "Loaded " << common_pairs.size() << " common pairs from "
                              << (std::string(opts.graph) + ".pairs") << "\n";
            } catch (const std::exception& e) {
                std::cout << "Warning: cannot load pairs: " << e.what() << " (ignoring)\n";
            }
        }

        // Build
        auto tstart = std::chrono::steady_clock::now();
        if (opts.size == 1) {
            Random rng(opts.seed);
            Size1Builder builder(G.number_of_vertices(), opts.from_vertex, opts.to_vertex,
                                 opts.colors, color_distribution, &rng, &out);
            builder.build();
        } else if (opts.threads == 1) {
            // Sequenziale (passo pairs + normalize)
            SequentialBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size,
                                      &ttc, opts.store0, selector, &out,
                                      common_pairs,           // NEW
                                      opts.normalize);        // NEW
            builder.build();
        } else {
            const PairSet* common_pairs_ptr = common_pairs.empty() ? nullptr : &common_pairs;
            // Multithread (qui mantieni la tua firma con normalize)
            MultithreadedBuilder builder(&G, opts.from_vertex, opts.to_vertex, opts.size,
                                         &ttc, opts.store0, selector, &out,
                                         opts.threads, opts.normalize, common_pairs_ptr); // NEW
            builder.build();
        }
        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
        std::cerr << "Building time: " << delta_t.count() << " s\n";

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        // Info file 
        PropertyStore properties;
        properties.set_bool("StoreOnlyOn0", opts.store0);
        if (color_distribution != nullptr)
            properties.set_double("ColoringProbability", pcold(color_distribution, opts.colors));
        if (opts.size == 1)
            properties.set_uint8("NumberOfColors", opts.colors);
        properties.save(std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".info");

        delete selector;
        for (unsigned i = 0; i < opts.size - 1; ++i) delete tables[i];
        delete[] readers;
        delete[] tables;

        return EXIT_SUCCESS;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}