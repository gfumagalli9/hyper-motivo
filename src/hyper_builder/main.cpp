// MIT License
// motivo-hyper-build — DP builder for hypergraphs (k >= 2), separated from graph builder.
// Adds --threads/-t and switches between HyperSequentialBuilder and HyperMultithreadedBuilder.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <thread>  
#include <chrono>

#include "../common/graph/Hypergraph.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/io/CompressedRecordFile.h"
#include "../common/random/Random.h"

#include "HyperSequentialBuilder.h"
#include "HyperMultithreadedBuilder.h"
#include "../common/treelets/TreeletStructureSelector.h"

// --------------------- CLI ---------------------
static void usage() {
    std::cerr <<
    "motivo-hyper-build\n"
    "  --graph|-g     <HIGH hypergraph basename>\n"
    "  --size|-s      <k> (k>=2)\n"
    "  --output|-o    <output basename>        # writes <output>.<k>.cnt\n"
    "  --lower        <GLOBAL TTC basename>    # reads <lower>.<i>.dtz     (i=1..k-1)\n"
    "  --ie           <HIGH IE TTC basename>   # reads <ie>.<i>.ie.dtz     (i=1..k-1)\n"
    "  [--normalize true|false]  (default: true)\n"
    "  [--store-on-0-only]       (default: off)\n"
    "  [--selector FILE]         (ignored for now)\n"
    "  [--threads|-t N]          (default: 1; 0 => use hardware_concurrency)\n";
}

// --------------------- TTC loader (RAII) ---------------------
using Reader = CompressedRecordFileReader<
    const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias>;

struct LoadedTTC {
    std::vector<std::unique_ptr<Reader>>       readers;
    std::vector<std::unique_ptr<TreeletTable>> tables;
    TreeletTableCollection                      coll{1}; // re-inited below
};

static void load_range_ttc(LoadedTTC& out,
                           const std::string& base,
                           unsigned upto_k_exclusive,
                           uint64_t num_vertices,
                           const char* ext)
{
    out.readers.clear();
    out.tables.clear();

    // Rebuild collection to exact capacity (tables 1..k-1)
    out.coll.~TreeletTableCollection();
    new (&out.coll) TreeletTableCollection(upto_k_exclusive ? (upto_k_exclusive - 1) : 0);

    for (unsigned i = 1; i < upto_k_exclusive; ++i) {
        auto r = std::make_unique<Reader>();
        const std::string path = base + "." + std::to_string(i) + ext;
        r->open(path);
        r->prefault(0, num_vertices ? (num_vertices - 1) : 0);

        auto t = std::make_unique<TreeletTable>(r.get());
        out.coll.add(t.get());

        out.readers.emplace_back(std::move(r));
        out.tables.emplace_back(std::move(t));
    }
}

// --------------------- MAIN ---------------------
int main(int argc, const char** argv) {
    const char* graph   = nullptr;   // HIGH hypergraph basename
    const char* output  = nullptr;   // out basename -> <output>.<k>.cnt
    const char* lower   = nullptr;   // GLOBAL TTC basename (1..k-1 -> .dtz)
    const char* iebase  = nullptr;   // HIGH   IE  basename (1..k-1 -> .ie.dtz)
    const char* selector_file = nullptr;

    unsigned k = 0;
    bool normalize = true;
    bool store_on_0 = false;
    unsigned threads = 1;            // <-- NEW

    // ---- parse ----
    for (int i=1; i<argc; ++i) {
        auto need = [&](int more){ if (i+more>=argc) { usage(); std::exit(1);} return argv[i+1]; };
        const char* a = argv[i];
        if (!std::strcmp(a,"--graph") || !std::strcmp(a,"-g"))       { graph  = need(1); ++i; }
        else if (!std::strcmp(a,"--size")  || !std::strcmp(a,"-s"))  { k      = (unsigned)std::stoul(need(1)); ++i; }
        else if (!std::strcmp(a,"--output")|| !std::strcmp(a,"-o"))  { output = need(1); ++i; }
        else if (!std::strcmp(a,"--lower"))                           { lower  = need(1); ++i; }
        else if (!std::strcmp(a,"--ie"))                              { iebase = need(1); ++i; }
        else if (!std::strcmp(a,"--normalize")) {
            std::string v = need(1); ++i;
            normalize = (v != "false" && v != "0" && v != "no");
        }
        else if (!std::strcmp(a,"--store-on-0-only")) { store_on_0 = true; }
        else if (!std::strcmp(a,"--selector"))        { selector_file = need(1); ++i; /* ignored for now */ }
        else if (!std::strcmp(a,"--threads") || !std::strcmp(a,"-t")) {
            threads = static_cast<unsigned>(std::stoul(need(1))); ++i;
        }
        else if (!std::strcmp(a,"--help") || !std::strcmp(a,"-h"))   { usage(); return 0; }
        else { std::cerr << "Unknown option: " << a << "\n"; usage(); return 1; }
    }

    if (!graph || !output || !lower || !iebase || k==0) { usage(); return 1; }
    if (k == 1) {
        std::cerr << "motivo-hyper-build: k=1 not handled here. Use the k=1 builder.\n";
        return 1;
    }

    // threads==0 -> hardware_concurrency (fallback to 1 if 0/not available)
    if (threads == 0) {
        unsigned hw = std::thread::hardware_concurrency();
        threads = hw ? hw : 1;
    }

    try {
        // Load HIGH hypergraph
        Hypergraph H(graph);
        const uint64_t NV = H.number_of_vertices();

        // Load TTC ranges:
        //  - lower: GLOBAL.<i>.dtz for i=1..k-1   (used by: ttc)
        //  - ie:    HIGH.<i>.ie.dtz for i=1..k-1  (used by: tIEc)
        LoadedTTC ttc_lower, ttc_ie;
        load_range_ttc(ttc_lower, lower, k, NV, ".dtz");
        load_range_ttc(ttc_ie,    iebase, k, NV, ".ie.dtz");

        // Output stream for <output>.<k>.cnt
        const std::string cnt_path = std::string(output) + "." + std::to_string(k) + ".cnt";
        std::ofstream ofs(cnt_path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            std::cerr << "Error: cannot open output " << cnt_path << "\n";
            return 2;
        }

        // Full vertex range
        Hypergraph::vertex_t from = 0;
        Hypergraph::vertex_t to   = (NV==0) ? 0 : (Hypergraph::vertex_t)(NV - 1);

        // (Selector non ancora utilizzato)
        TreeletStructureSelector* selector = nullptr;
        (void)selector_file;

        std::cout << "Computing size-" << k << " on [" << from << ".." << to
                  << "] using " << threads << " thread(s)\n";

        const auto t0 = std::chrono::steady_clock::now();

        if (threads <= 1) {
            HyperSequentialBuilder builder(&H, from, to,
                                           k,
                                           &ttc_lower.coll,
                                           &ttc_ie.coll,
                                           store_on_0,
                                           selector,
                                           &ofs,
                                           normalize);
            builder.build();
        } else {
            HyperMultithreadedBuilder builder(&H, from, to,
                                              k,
                                              &ttc_lower.coll,
                                              &ttc_ie.coll,
                                              store_on_0,
                                              selector,
                                              &ofs,
                                              threads,
                                              normalize);
            builder.build();
        }

        ofs.flush();

        const auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double>(t1 - t0).count();
        std::cerr << "Building time: " << secs << " s\n";
        std::cerr << "Wrote " << cnt_path << "\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}