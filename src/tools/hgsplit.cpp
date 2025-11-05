// MIT License
//
// Hypergraph split tool
// ---------------------
// Given an input hypergraph, split its hyperedges into two parts based on a
// size threshold T:
//   - "small":  hyperedges with |e| <= T
//   - "large":  hyperedges with |e| >  T
//
// Then compute the set of vertex pairs (u,v) that:
//   (a) co-occur in at least one "small" hyperedge, and
//   (b) also co-occur in at least one "large" hyperedge.
// Pairs are deduplicated and written to <small_output>.pairs as a compact
// binary blob: [uint64_t M][(u,v) repeated M times].
//
// Outputs:
//   <small>.{hmeta,hef,hvd,vhef,vhed}  : small-part hypergraph (binary format)
//   <large>.{hmeta,hef,hvd,vhef,vhed}  : large-part hypergraph (binary format)
//   <small>.pairs                       : pairs common to small∩large
//
// Notes:
//   * Adjacency in .vhed and .hef is stored sorted for determinism.
//   * Pair order is normalized (u < v) before dedup.
//
// CLI:
//   --input|-i        <basename>
//   --threshold|-t    <T>   (max hyperedge size for "small"; default: 0)
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

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"
#include "../tools/alpha_beta.cpp"

using vertex_t = Hypergraph::vertex_t;
using edge_t   = Hypergraph::edge_t;

// Build, for each vertex u, the (sorted, duplicate-free) list of incident
// hyperedge IDs in the "large" hypergraph. This enables fast intersection
// tests between two vertices' incident sets.
static std::vector<std::vector<edge_t>>
build_high_incidence(const Hypergraph& H_high)
{
    const std::uint32_t n = H_high.number_of_vertices();
    std::vector<std::vector<edge_t>> inc(n);

    for (vertex_t u = 0; u < n; ++u) {
        const std::uint32_t deg = H_high.vertex_degree(u);
        auto& lst = inc[u];
        lst.reserve(deg);
        for (std::uint32_t i = 0; i < deg; ++i) {
            const edge_t e = H_high.incident_hyperedge(u, i);
            lst.push_back(e);
        }
        std::sort(lst.begin(), lst.end());
        lst.erase(std::unique(lst.begin(), lst.end()), lst.end());
    }
    return inc;
}

// Return true if two sorted vectors of edge IDs share at least one common ID.
static inline bool share_any_high_edge(const std::vector<edge_t>& a,
                                       const std::vector<edge_t>& b)
{
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) return true;
        (a[i] < b[j]) ? ++i : ++j;
    }
    return false;
}

// Write a hypergraph (given as explicit hyperedges + vertex count) to the
// standard Motivo binary format: {hmeta, hef, hvd, vhef, vhed}.
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

    // HEF/HVD: hyperedge->vertices
    std::vector<std::uint32_t> offsets_he(num_edges + 1);
    std::vector<vertex_t>      hvd;
    std::uint32_t off = 0;
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<std::uint32_t>(hyperedges[e].size());
        for (vertex_t v : hyperedges[e]) hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // VHEF/VHED: vertex->hyperedges (sorted for determinism)
    std::vector<std::uint32_t> offsets_vh(num_verts + 1);
    std::vector<edge_t>        vhed;
    off = 0;
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto& inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<std::uint32_t>(inc.size());
        for (edge_t e : inc) vhed.push_back(e);
    }
    offsets_vh[num_verts] = off;

    // Emit files
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hmeta");
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hef");
        f.write(reinterpret_cast<const char*>(offsets_he.data()),
                static_cast<std::streamsize>(offsets_he.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hvd");
        f.write(reinterpret_cast<const char*>(hvd.data()),
                static_cast<std::streamsize>(hvd.size() * sizeof(vertex_t)));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhef");
        f.write(reinterpret_cast<const char*>(offsets_vh.data()),
                static_cast<std::streamsize>(offsets_vh.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhed");
        f.write(reinterpret_cast<const char*>(vhed.data()),
                static_cast<std::streamsize>(vhed.size() * sizeof(edge_t)));
    }
}

int main(int argc, const char** argv)
{
    OptionsParser op;
    auto* help_opt   = op.add_option(false, false, "help",         'h', "",  "Print help and exit");
    auto* input_opt  = op.add_option(true,  true,  "input",        'i', "",  "Input binary hypergraph basename");
    auto* thresh_opt = op.add_option(false,  true,  "threshold",    't', "", "Maximum hyperedge size for the small hypergraph");    
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
        // 1) Load input hypergraph and split edges in memory
        Hypergraph H(in_base);

        // Compute the threshold value:
        // - If --threshold is provided with "auto"/"AUTO" or empty: use auto-α
        // - Else parse an unsigned integer; on invalid input, print a clear error and exit.
        vertex_t threshold = 0;
        {
            bool use_auto = true;
            if (thresh_opt->is_found()) {
                const std::string val = thresh_opt->get_value();
                if (!val.empty() && val != "auto" && val != "AUTO") {
                    try {
                        // Parse as unsigned; clamp to vertex_t range.
                        const unsigned long long t_raw = std::stoull(val);
                        const unsigned long long TMAX  = std::numeric_limits<vertex_t>::max();
                        threshold = static_cast<vertex_t>(std::min<unsigned long long>(t_raw, TMAX));
                        use_auto = false;
                        std::cout << "Split alpha (manual): " << threshold << "\n";
                    } catch (const std::exception&) {
                        std::cerr << "Invalid --threshold value: '" << val
                                << "'. Use an integer, or 'auto'.\n";
                        return EXIT_FAILURE;
                    }
                }
            }
            if (use_auto) {
                const std::size_t a = motivo::compute_best_alpha(H, 0.5);
                const std::size_t TMAX = std::numeric_limits<vertex_t>::max();
                threshold = static_cast<vertex_t>(std::min<std::size_t>(a, TMAX));
                std::cout << "Split alpha (auto): " << threshold << "\n";
            }
        }

        const edge_t m = H.number_of_hyperedges();

        std::vector<std::vector<vertex_t>> small_he;
        std::vector<std::vector<vertex_t>> large_he;
        small_he.reserve(m); // rough upper bound; vectors will reclaim space as needed
        large_he.reserve(m);

        for (edge_t e = 0; e < m; ++e) {
            const std::uint32_t sz = H.hyperedge_size(e);
            std::vector<vertex_t> he(sz);
            for (std::uint32_t i = 0; i < sz; ++i) he[i] = H.hyperedge_vertex(e, i);
            if (sz <= threshold) small_he.push_back(std::move(he)); // TO FIX: if chosen treshold makes lower or higher empty it crashes
            else                 large_he.push_back(std::move(he));
        }

        // 2) Write the two parts as standard binary hypergraphs
        write_hypergraph(out_small, small_he, H.number_of_vertices());
        write_hypergraph(out_large, large_he, H.number_of_vertices());

        // 3) Reload both parts through the canonical loader (ensures format sanity)
        Hypergraph H_large(out_large);
        Hypergraph H_small(out_small);

        // If one side is empty, the intersection pairs are necessarily empty.
        if (H_small.number_of_hyperedges() == 0 || H_large.number_of_hyperedges() == 0) {
            const std::string filename = out_small + ".pairs";
            std::ofstream out(filename, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open pairs output file: " + filename);
            const std::uint64_t M = 0;
            out.write(reinterpret_cast<const char*>(&M), sizeof(M));
            return EXIT_SUCCESS;
        }

        // 4) Build per-vertex incident-edge lists for the "large" part (sorted)
        auto high_inc = build_high_incidence(H_large);

        // 5) Extract pairs only from "small" hyperedges; keep those that also share
        //    at least one "large" hyperedge (i.e., small∩large pairs).
        std::vector<std::pair<vertex_t, vertex_t>> pairs;
        pairs.reserve(1024); // optional heuristic

        const edge_t m_low = H_small.number_of_hyperedges();
        for (edge_t e = 0; e < m_low; ++e) {
            const std::uint32_t sz = H_small.hyperedge_size(e);
            if (sz < 2) continue;

            std::vector<vertex_t> vs;
            vs.reserve(sz);
            for (std::uint32_t j = 0; j < sz; ++j)
                vs.push_back(H_small.hyperedge_vertex(e, j));

            // Generate all unordered pairs from this small hyperedge
            for (std::uint32_t i = 0; i + 1 < vs.size(); ++i) {
                for (std::uint32_t j = i + 1; j < vs.size(); ++j) {
                    vertex_t u = vs[i], v = vs[j];
                    if (u > v) std::swap(u, v); // normalize order

                    if (share_any_high_edge(high_inc[u], high_inc[v])) {
                        pairs.emplace_back(u, v);
                    }
                }
            }
        }

        // 6) Global dedup and write compact binary blob
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

        {
            const std::string filename = out_small + ".pairs";
            std::ofstream out(filename, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open pairs output file: " + filename);

            const std::uint64_t M = static_cast<std::uint64_t>(pairs.size());
            out.write(reinterpret_cast<const char*>(&M), sizeof(M));
            for (const auto& p : pairs) {
                out.write(reinterpret_cast<const char*>(&p.first),  sizeof(p.first));
                out.write(reinterpret_cast<const char*>(&p.second), sizeof(p.second));
            }
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}