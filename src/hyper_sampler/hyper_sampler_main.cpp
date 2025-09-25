// MIT License
// =============================================================
// motivo-hyper-sample: Non-adaptive sampler for hypergraphs
// Uses HyperOccurrenceSampler + ROOT TTC + NWS_HIGH TTC.
// No LOW/HIGH TTCs, no smart-stars, no adaptive.
// =============================================================

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "config.h"

#include "../common/io/PropertyStore.h"
#include "../common/platform/platform.h"
#include "../common/random/Random.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/util.h"

#include "../common/graph/Hypergraph.h"
#include "../common/graph/UndirectedGraph.h"

#include "HyperOccurrenceSampler.h"
#include "HyperSampleTable.h"

// ------------------------- Options -------------------------
struct hyper_opts {
    // inputs
    char gaifman_graph[MOTIVO_ARG_MAX]   = {0}; // LOW (Gaifman)
    char hypergraph[MOTIVO_ARG_MAX]      = {0}; // HIGH
    char hypergraph_full[MOTIVO_ARG_MAX] = {0}; // FULL (LOW size-2 + HIGH)

    // tables
    char tables_basename[MOTIVO_ARG_MAX]  = {0}; // ROOT (for root sampler)
    char nws_high_basename[MOTIVO_ARG_MAX]= {0}; // NWS HIGH (for HIGH part)

    // output/seed
    char output_basename[MOTIVO_ARG_MAX] = {0};
    char seed[MOTIVO_ARG_MAX]            = {0};

    // numeric
    unsigned int size              = 0;   // k
    uint64_t     number_of_samples = 0;   // 0 -> time-budget mode if time_budget>0
    unsigned int threads           = 1;
    double       time_budget       = std::numeric_limits<double>::infinity();

    // flags
    bool vertices       = true;
    bool graphlets      = true;   // weakly-induced if true; treelet-only if false
    bool canonicize     = false;  // canonicalize incidence (V/E)
    bool group          = false;  // group by footprint
    bool spanning_trees = false;  // count rooted spanning trees on Gaifman
    bool estimate_occ   = true;   // estimate global occurrences (-> frequencies)
    bool save_samples   = false;  // save raw samples to <basename>.samples.csv
};

// ------------------------- CLI -------------------------
static void print_usage() {
    std::cerr <<
        "motivo-hyper-sample [OPTIONS]\n"
        "  --gaifman <gaifman_graph>\n"
        "  --hypergraph <hypergraph_high>\n"
        "  --hypergraph-full <hypergraph_full>\n"
        "  --tables <basename_root>\n"
        "  --nws-high <basename_nws>\n"
        "  --size <k>\n"
        "  [--num-samples N] [--time-budget SEC]\n"
        "  [--threads T] [--seed STR]\n"
        "  [--vertices] [--graphlets] [--canonicize]\n"
        "  [--group] [--spanning-trees] [--estimate-occurrences]\n"
        "  [--save-samples]\n"
        "  [--output BASENAME]\n";
}

static bool parse_args(int argc, const char** argv, hyper_opts* o) {
    if (argc < 2) { print_usage(); return false; }

    auto need = [&](int i, int more, const char** av){
        if (i + more >= argc) { print_usage(); std::exit(EXIT_FAILURE); }
        return av[i+1];
    };

    for (int i=1; i<argc; ++i) {
        const char* a = argv[i];
        if (!strcmp(a,"--gaifman"))              { strncpy(o->gaifman_graph, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--hypergraph"))      { strncpy(o->hypergraph, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--hypergraph-full")) { strncpy(o->hypergraph_full, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--tables"))          { strncpy(o->tables_basename, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--nws-high"))        { strncpy(o->nws_high_basename, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--size"))            { o->size = static_cast<unsigned>(std::stoul(need(i,1,argv))); ++i; }
        else if (!strcmp(a,"--num-samples"))     { o->number_of_samples = std::stoull(need(i,1,argv)); ++i; }
        else if (!strcmp(a,"--time-budget"))     { o->time_budget = std::stod(need(i,1,argv)); ++i; }
        else if (!strcmp(a,"--threads")) {
            unsigned t = static_cast<unsigned>(std::stoul(need(i,1,argv))); ++i;
            o->threads = t ? t : std::thread::hardware_concurrency();
            if (o->threads==0) o->threads = 1;
        }
        else if (!strcmp(a,"--seed"))            { strncpy(o->seed, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--vertices"))        { o->vertices = true; }
        else if (!strcmp(a,"--graphlets"))       { o->graphlets = true; }
        else if (!strcmp(a,"--canonicize"))      { o->canonicize = true; }
        else if (!strcmp(a,"--group"))           { o->group = true; }
        else if (!strcmp(a,"--spanning-trees"))  { o->spanning_trees = true; }
        else if (!strcmp(a,"--estimate-occurrences")) { o->estimate_occ = true; }
        else if (!strcmp(a,"--save-samples"))    { o->save_samples = true; }
        else if (!strcmp(a,"--output"))          { strncpy(o->output_basename, need(i,1,argv), MOTIVO_ARG_MAX-1); ++i; }
        else if (!strcmp(a,"--help") || !strcmp(a,"-h")) { print_usage(); return false; }
        else { std::cerr << "Unknown option: " << a << "\n"; print_usage(); return false; }
    }

    // required fields
    if (*o->gaifman_graph=='\0' || *o->hypergraph=='\0' || *o->hypergraph_full=='\0' ||
        *o->tables_basename=='\0' || *o->nws_high_basename=='\0' || o->size==0) {
        std::cerr << "Missing required options.\n"; print_usage(); return false;
    }
    if (o->number_of_samples==0 && !(o->time_budget>0)) {
        std::cerr << "Provide --num-samples or --time-budget.\n"; return false;
    }
    return true;
}

// ------------------------- TTC loaders (RAII) -------------------------
using Reader = CompressedRecordFileReader<
    const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias>;

struct LoadedTTC {
    std::vector<std::unique_ptr<Reader>>       readers;
    std::vector<std::unique_ptr<TreeletTable>> tables;
    TreeletTableCollection                      coll;
};

static void load_ttc(LoadedTTC& out,
                     const std::string& basename,
                     unsigned k,
                     uint64_t num_vertices,
                     const char* ext = ".dtz")
{
    out.readers.clear();
    out.tables.clear();
    out.readers.reserve(k);
    out.tables.reserve(k);

    for (unsigned i = 1; i <= k; ++i) {
        auto r = std::make_unique<Reader>();
        const std::string path = basename + "." + std::to_string(i) + ext;
        r->open(path);
        r->prefault(0, num_vertices ? (num_vertices - 1) : 0);

        auto t = std::make_unique<TreeletTable>(r.get());
        out.coll.add(t.get());

        out.readers.emplace_back(std::move(r));
        out.tables.emplace_back(std::move(t));
    }
}

static void load_root_ttc(LoadedTTC& out,
                          const std::string& basename,
                          unsigned k,
                          uint64_t num_vertices)
{
    load_ttc(out, basename, k, num_vertices, ".dtz");
    const std::string rts = basename + "." + std::to_string(k) + ".rts";
    out.tables.back()->load_root_sampler(rts);
}

// ------------------------------ MAIN ------------------------------
int main(int argc, const char** argv)
{
    std::cerr << "This is motivo-hyper-sample. Version: "
              << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

    try {
        hyper_opts opts{};
        if (!parse_args(argc, argv, &opts))
            return EXIT_SUCCESS;

        // Root properties (as in motivo-sample)
        PropertyStore properties(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".info");
        const bool store_only_on_0 = properties.get_bool("StoreOnlyOn0", false);
        const uint128_t tot_colorful_treelets =
            properties.get_uint128("TotTreelets", 0) * (store_only_on_0 ? opts.size : 1);

        PropertyStore properties1(std::string(opts.tables_basename) + ".1.info");
        const uint8_t colors = properties1.get_uint8("NumberOfColors", 0);
        const double  p = properties.get_double("ColoringProbability",
                                                pcol(opts.size, colors ? colors : opts.size));

        // Load graphs
        UndirectedGraph G_low(opts.gaifman_graph);
        G_low.prefault();
        std::cerr << "Loaded Gaifman graph with " << G_low.number_of_vertices()
                  << " vertices and " << G_low.number_of_edges() << " edges\n";

        Hypergraph H_high(opts.hypergraph);
        std::cerr << "Loaded HIGH hypergraph with " << H_high.number_of_vertices()
                  << " vertices and " << H_high.number_of_hyperedges() << " hyperedges\n";

        Hypergraph H_full(opts.hypergraph_full);
        std::cerr << "Loaded FULL hypergraph with " << H_full.number_of_vertices()
                  << " vertices and " << H_full.number_of_hyperedges() << " hyperedges\n";

        if (G_low.number_of_vertices() != H_high.number_of_vertices()
         || G_low.number_of_vertices() != H_full.number_of_vertices()) {
            std::cerr << "Error: all inputs must have the same number of vertices.\n";
            return EXIT_FAILURE;
        }
        const uint64_t NV = G_low.number_of_vertices();

        // Load TTCs: only ROOT and NWS(HIGH)
        LoadedTTC root, nws;
        std::cerr << "Loading ROOT C tables (for root sampler)\n";
        load_root_ttc(root, opts.tables_basename, opts.size, NV);

        std::cerr << "Loading NWS HIGH tables\n";
        load_ttc(nws, opts.nws_high_basename, opts.size, NV, ".ie.dtz");

        // RNG
        Random rng(opts.seed);
        std::cerr << "Using seed " << rng.get_seed() << std::endl;

        // Output sinks
        std::unique_ptr<std::ofstream> out_file;
        std::ostream* out = &std::cout;
        std::unique_ptr<std::ofstream> out_raw_file;

        // Final CSV -> <basename>.csv (if requested)
        if (*opts.output_basename) {
            out_file = std::make_unique<std::ofstream>(
                std::string(opts.output_basename) + ".csv",
                std::ofstream::binary | std::ofstream::trunc);
            out = out_file.get();
        }
        // Raw samples snapshot (pre post-processing) -> <basename>.samples.csv
        if (opts.save_samples) {
            const std::string base = *opts.output_basename
                ? std::string(opts.output_basename)
                : std::string("hyper-samples");
            out_raw_file = std::make_unique<std::ofstream>(
                base + ".samples.csv",
                std::ofstream::binary | std::ofstream::trunc);
        }

        // Sampling
        std::cerr << "Sampling using " << opts.threads << " thread(s)\n";
        const auto tstart = std::chrono::steady_clock::now();

        // H_large = HIGH, H = FULL
        HyperOccurrenceSampler sampler(&G_low, &H_high, &H_full,
                                       &root.coll, &nws.coll,
                                       opts.size,
                                       opts.vertices, opts.graphlets, opts.canonicize);

        const uint64_t nsamples = opts.number_of_samples; // 0 -> time-budget mode
        std::unique_ptr<HyperSampleTable> samples(
            sampler.sample(nsamples, opts.threads, &rng, opts.time_budget)
        );

        const auto elapsed = std::chrono::steady_clock::now() - tstart;
        std::cerr << "Hyper sampler: took " << samples->get_num_samples()
                  << " samples in " << std::chrono::duration<double>(elapsed).count() << " s\n";

        // Optional raw dump (pre processing)
        if (out_raw_file) {
            *out_raw_file << HyperSampleTable::header << "\n" << *samples;
            out_raw_file->flush();
        }

        // Post-processing
        if (opts.group || opts.spanning_trees) samples->sort_by_footprint();
        if (opts.group)                        samples->group_by_footprint();
        if (opts.spanning_trees)               samples->count_rooted_spanning_trees(nullptr, opts.threads);

        if (opts.estimate_occ) {
            samples->estimate_occurrences(static_cast<double>(tot_colorful_treelets) / p);
            samples->estimate_frequencies();
            samples->sort_by_estimate_occurrences();
        } else {
            samples->estimate_frequencies();
        }

        // Final CSV
        *out << HyperSampleTable::header << "\n" << *samples;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}