// MIT License
//
// Hypergraph split tool (optimized)
// ---------------------------------
// Split the input hypergraph into SMALL (|e| <= T) and LARGE (|e| > T),
// and compute .pairs = { (u,v) | u<v, (u,v) co-occur in >=1 SMALL he AND >=1 LARGE he }.
//
// Optimizations vs. previous version:
//  * No write-then-reload: .pairs is computed directly from in-memory SMALL/LARGE.
//  * Per-vertex token sweep: for each u, mark SMALL-neighbors, then scan LARGE
//    incident hyperedges to emit (u,v) only once (v>u), with per-u sorting.
//  * Output pairs globally ordered by u then v, no global sort/unique needed.
//
// Output files:
//   <small>.{hmeta,hef,hvd,vhef,vhed} : SMALL hypergraph (binary formato Motivo)
//   <large>.{hmeta,hef,hvd,vhef,vhed} : LARGE hypergraph
//   <small>.pairs                      : [uint64_t M][(u,v) repeated M times], with u<v
//
// CLI:
//   --input|-i        <basename>
//   --threshold|-t    <T>  (integer) or "auto" (default: auto)
//   --small-output|-s <basename>
//   --large-output|-l <basename>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"
#include "../tools/alpha_beta.cpp"

using vertex_t = Hypergraph::vertex_t;
using edge_t   = Hypergraph::edge_t;

// -------------------------- writers --------------------------

static void write_hypergraph(const std::string& basename,
                             const std::vector<std::vector<vertex_t>>& hyperedges,
                             const vertex_t num_verts)
{
    const edge_t num_edges = static_cast<edge_t>(hyperedges.size());

    // Build vertex->edges incidence (unsorted initially)
    std::vector<std::vector<edge_t>> v2e(num_verts);
    for (edge_t e = 0; e < num_edges; ++e)
        for (vertex_t v : hyperedges[e])
            v2e[v].push_back(e);

    // HEF/HVD: hyperedge->vertices (concatenated)
    std::vector<std::uint32_t> offsets_he(num_edges + 1);
    std::vector<vertex_t>      hvd;
    hvd.reserve(
        std::accumulate(hyperedges.begin(), hyperedges.end(), std::size_t{0},
                        [](std::size_t s, const auto& he){ return s + he.size(); })
    );
    std::uint32_t off = 0;
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<std::uint32_t>(hyperedges[e].size());
        hvd.insert(hvd.end(), hyperedges[e].begin(), hyperedges[e].end());
    }
    offsets_he[num_edges] = off;

    // VHEF/VHED: vertex->hyperedges (sorted for determinism)
    std::vector<std::uint32_t> offsets_vh(num_verts + 1);
    std::vector<edge_t>        vhed;
    vhed.reserve(hvd.size()); // rough
    off = 0;
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto& inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<std::uint32_t>(inc.size());
        vhed.insert(vhed.end(), inc.begin(), inc.end());
    }
    offsets_vh[num_verts] = off;

    // Emit files
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hmeta for " + basename);
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hef for " + basename);
        f.write(reinterpret_cast<const char*>(offsets_he.data()),
                static_cast<std::streamsize>(offsets_he.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hvd for " + basename);
        f.write(reinterpret_cast<const char*>(hvd.data()),
                static_cast<std::streamsize>(hvd.size() * sizeof(vertex_t)));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhef for " + basename);
        f.write(reinterpret_cast<const char*>(offsets_vh.data()),
                static_cast<std::streamsize>(offsets_vh.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhed for " + basename);
        f.write(reinterpret_cast<const char*>(vhed.data()),
                static_cast<std::streamsize>(vhed.size() * sizeof(edge_t)));
    }
}

// -------------------------- main -----------------------------

int main(int argc, const char** argv)
{
    OptionsParser op;
    auto* help_opt   = op.add_option(false, false, "help",         'h', "",  "Print help and exit");
    auto* input_opt  = op.add_option(true,  true,  "input",        'i', "",  "Input binary hypergraph basename");
    auto* thresh_opt = op.add_option(false, true,  "threshold",    't', "",  "Maximum hyperedge size for the small hypergraph (or 'auto')");
    auto* small_opt  = op.add_option(true,  true,  "small-output", 's', "",  "Output basename for hyperedges of size <= threshold");
    auto* large_opt  = op.add_option(true,  true,  "large-output", 'l', "",  "Output basename for hyperedges of size > threshold");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Missing required options\n";
        return EXIT_FAILURE;
    }

    const std::string in_base   = input_opt->get_value();
    const std::string out_small = small_opt->get_value();
    const std::string out_large = large_opt->get_value();

    try {
        // 1) Load input hypergraph
        Hypergraph H(in_base);
        const vertex_t n = H.number_of_vertices();
        const edge_t   m = H.number_of_hyperedges();

        // 2) Threshold (auto or manual)
        vertex_t threshold = 0;
        {
            bool use_auto = true;
            if (thresh_opt->is_found()) {
                const std::string val = thresh_opt->get_value();
                if (!val.empty() && val != "auto" && val != "AUTO") {
                    try {
                        const unsigned long long t_raw = std::stoull(val);
                        const unsigned long long TMAX  = std::numeric_limits<vertex_t>::max();
                        threshold = static_cast<vertex_t>(std::min<unsigned long long>(t_raw, TMAX));
                        use_auto = false;
                        std::cout << "Split alpha (manual): " << threshold << "\n";
                    } catch (...) {
                        std::cerr << "Invalid --threshold value: '" << val
                                  << "'. Use an integer, or 'auto'.\n";
                        return EXIT_FAILURE;
                    }
                }
            }
            if (use_auto) {
                const std::size_t a = motivo::compute_best_alpha(H, 0.01);
                const std::size_t TMAX = std::numeric_limits<vertex_t>::max();
                threshold = static_cast<vertex_t>(std::min<std::size_t>(a, TMAX));
                std::cout << "Split alpha (auto): " << threshold << "\n";
            }
        }

        // 3) Split in memory: SMALL/LARGE hyperedges + per-vertex incidence (indices local to each side)
        std::vector<std::vector<vertex_t>> small_he;
        std::vector<std::vector<vertex_t>> large_he;
        small_he.reserve(m); large_he.reserve(m);

        std::vector<std::vector<edge_t>> v2e_small(n), v2e_large(n);

        for (edge_t e = 0; e < m; ++e) {
            const std::uint32_t sz = H.hyperedge_size(e);
            std::vector<vertex_t> he(sz);
            for (std::uint32_t i = 0; i < sz; ++i) he[i] = H.hyperedge_vertex(e, i);

            // (facoltativo) garantisci uniqueness per iperarco
            // std::sort(he.begin(), he.end()); he.erase(std::unique(he.begin(), he.end()), he.end());

            if (sz <= threshold) {
                edge_t id = static_cast<edge_t>(small_he.size());
                small_he.push_back(std::move(he));
                for (vertex_t v : small_he.back()) v2e_small[v].push_back(id);
            } else {
                edge_t id = static_cast<edge_t>(large_he.size());
                large_he.push_back(std::move(he));
                for (vertex_t v : large_he.back()) v2e_large[v].push_back(id);
            }
        }

        // 4) Compute pairs directly (per-vertex sweep)
        // Token arrays (uint32) to avoid memset(n) per vertex
        std::vector<std::uint32_t> seen_small(n, 0), emitted(n, 0);
        std::uint32_t token = 1;

        std::vector<std::pair<vertex_t, vertex_t>> pairs;
        pairs.reserve(1024 * 1024); // heuristic; grows if needed

        for (vertex_t u = 0; u < n; ++u, ++token) {
            if (token == 0) {
                std::fill(seen_small.begin(), seen_small.end(), 0);
                std::fill(emitted.begin(),    emitted.end(),    0);
                token = 1;
            }

            // 4.1) Mark SMALL-neighbors of u (union of all SMALL hyperedges incident to u)
            for (edge_t e : v2e_small[u]) {
                const auto& he = small_he[e];
                for (vertex_t v : he) {
                    if (v != u) seen_small[v] = token;
                }
            }

            // 4.2) Traverse LARGE hyperedges of u, emit (u,v) once per v>u that is marked
            std::vector<vertex_t> out_vs;
            for (edge_t e : v2e_large[u]) {
                const auto& he = large_he[e];
                for (vertex_t v : he) {
                    if (v > u && seen_small[v] == token && emitted[v] != token) {
                        emitted[v] = token;
                        out_vs.push_back(v);
                    }
                }
            }

            if (!out_vs.empty()) {
                std::sort(out_vs.begin(), out_vs.end());
                out_vs.erase(std::unique(out_vs.begin(), out_vs.end()), out_vs.end());
                // append as (u,v), maintaining global order by u then v
                for (vertex_t v : out_vs) pairs.emplace_back(u, v);
            }
        }

        // 5) Write SMALL/LARGE hypergraphs (binary) and the .pairs
        write_hypergraph(out_small, small_he, n);
        write_hypergraph(out_large, large_he, n);

        {
            const std::string filename = out_small + ".pairs";
            std::ofstream out(filename, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open pairs output file: " + filename);

            const std::uint64_t M = static_cast<std::uint64_t>(pairs.size());
            out.write(reinterpret_cast<const char*>(&M), sizeof(M));
            for (const auto& p : pairs) {
                out.write(reinterpret_cast<const char*>(&p.first),  sizeof(vertex_t));
                out.write(reinterpret_cast<const char*>(&p.second), sizeof(vertex_t));
            }
        }

        std::cout << "Split done. SMALL edges: " << small_he.size()
                  << ", LARGE edges: " << large_he.size()
                  << ", pairs (small∩large): " << pairs.size() << "\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}