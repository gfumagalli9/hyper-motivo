// MIT License
//
// Gaifman graph construction tool: from a hypergraph to an undirected simple graph.
// - Each hyperedge induces a clique among its vertices.
// - Optional filter: only hyperedges with size <= M (useful to build a Gaifman-LOW).
// - Parallel build with std::thread (no OpenMP).
//
// Output binary graph format (compatible with the rest of Motivo):
//   out_base.gof  : [num_verts:V][num_undirected_edges:V][offsets[0..n] as V]
//                   offsets[i] is the starting index of vertex i's adjacency in .ged.
//                   The last sentinel offsets[n] equals the total number of stored neighbors.
//   out_base.ged  : flat array of neighbors (V) for all vertices, concatenated.
//                   For each vertex i, neighbors are stored in ascending order and without duplicates.

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"

using V = Hypergraph::vertex_t;
using E = Hypergraph::edge_t;

//------------------------------------------------------------------------------
// Binary writer: emits the adjacency as (offsets + edges) in Motivo's format.
// - adj[u] must be sorted and duplicate-free.
// - undirected edge count is inferred as sum(deg)/2.
//------------------------------------------------------------------------------
static void write_graph_bin(const std::string& out_base,
                            const std::vector<std::vector<V>>& adj)
{
    const V n = static_cast<V>(adj.size());

    // Sum of (directed) degrees; each undirected edge appears twice
    std::uint64_t deg_sum = 0;
    for (const auto& nb : adj) deg_sum += nb.size();
    const V undirected_edges = static_cast<V>(deg_sum / 2);

    std::ofstream gof(out_base + ".gof", std::ios::binary);
    std::ofstream ged(out_base + ".ged", std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header: [num_vertices][num_undirected_edges]
    gof.write(reinterpret_cast<const char*>(&n), sizeof(V));
    gof.write(reinterpret_cast<const char*>(&undirected_edges), sizeof(V));

    // Offsets + neighbors payload
    std::uint64_t written = 0;
    for (V u = 0; u < n; ++u) {
        const V off = static_cast<V>(written);
        gof.write(reinterpret_cast<const char*>(&off), sizeof(V));

        if (!adj[u].empty()) {
            ged.write(reinterpret_cast<const char*>(adj[u].data()),
                      static_cast<std::streamsize>(adj[u].size() * sizeof(V)));
            written += adj[u].size();
        }
    }

    // Final sentinel offset (offsets[n])
    const V final_off = static_cast<V>(written);
    gof.write(reinterpret_cast<const char*>(&final_off), sizeof(V));
}

//------------------------------------------------------------------------------
// Serial Gaifman construction.
// For each vertex u, we visit incident hyperedges (optionally filtered by max_m)
// and add all co-vertices v != u once (dedup via "seen" token technique).
// Adjacency lists are sorted for determinism.
//------------------------------------------------------------------------------
static std::vector<std::vector<V>>
build_gaifman_serial(const Hypergraph& H, std::uint32_t max_m /* 0 = disabled */)
{
    const V n = H.number_of_vertices();
    std::vector<std::vector<V>> adj(n);

    // Token-based "seen" array for O(1) dedup across a vertex's neighborhood
    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    for (V u = 0; u < n; ++u, ++token) {
        // Token overflow guard: if token wraps to 0, reset the bitmap
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

        // Capacity hint: sum(|e|-1) over incident hyperedges (respecting filter)
        std::uint64_t cap = 0;
        const std::uint32_t deg = H.vertex_degree(u);
        for (std::uint32_t i = 0; i < deg; ++i) {
            const E e = H.incident_hyperedge(u, i);
            const std::uint32_t sz = H.hyperedge_size(e);
            if (max_m && sz > max_m) continue;
            cap += (sz > 0 ? (sz - 1) : 0);
        }

        auto& nb = adj[u];
        nb.reserve(nb.size() + static_cast<size_t>(cap));

        // Fill adjacency (unique neighbors)
        for (std::uint32_t i = 0; i < deg; ++i) {
            const E e = H.incident_hyperedge(u, i);
            const std::uint32_t sz = H.hyperedge_size(e);
            if (max_m && sz > max_m) continue;

            for (std::uint32_t j = 0; j < sz; ++j) {
                const V v = H.hyperedge_vertex(e, j);
                if (v == u) continue;
                if (seen[v] != token) { seen[v] = token; nb.push_back(v); }
            }
        }

        std::sort(nb.begin(), nb.end()); // determinism
    }
    return adj;
}

//------------------------------------------------------------------------------
// Parallel Gaifman construction (std::thread).
// We assign contiguous vertex ranges to threads. Each thread uses its own
// "seen" bitmap and a local vector for the current u, so there is no sharing
// or locking. Output slots adj[u] are disjoint (no races).
//------------------------------------------------------------------------------
static std::vector<std::vector<V>>
build_gaifman_threads(const Hypergraph& H, std::uint32_t max_m, int threads)
{
    const V n = H.number_of_vertices();
    if (n == 0) return {};

    if (threads <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threads = hw ? static_cast<int>(hw) : 1;
    }
    threads = std::max(1, std::min(threads, static_cast<int>(n)));

    std::vector<std::vector<V>> adj(n);
    std::vector<std::thread> pool;
    pool.reserve(threads);

    auto worker = [&](V start, V end) {
        std::vector<std::uint32_t> seen(n, 0);
        std::uint32_t token = 1;

        for (V u = start; u < end; ++u, ++token) {
            if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

            // Capacity hint for u
            std::uint64_t cap = 0;
            const std::uint32_t deg = H.vertex_degree(u);
            for (std::uint32_t i = 0; i < deg; ++i) {
                const E e = H.incident_hyperedge(u, i);
                const std::uint32_t sz = H.hyperedge_size(e);
                if (max_m && sz > max_m) continue;
                cap += (sz > 0 ? (sz - 1) : 0);
            }

            std::vector<V> nb;
            nb.reserve(static_cast<size_t>(cap));

            for (std::uint32_t i = 0; i < deg; ++i) {
                const E e = H.incident_hyperedge(u, i);
                const std::uint32_t sz = H.hyperedge_size(e);
                if (max_m && sz > max_m) continue;

                for (std::uint32_t j = 0; j < sz; ++j) {
                    const V v = H.hyperedge_vertex(e, j);
                    if (v == u) continue;
                    if (seen[v] != token) { seen[v] = token; nb.push_back(v); }
                }
            }

            std::sort(nb.begin(), nb.end());
            adj[u] = std::move(nb); // thread-safe: unique u per worker
        }
    };

    // Block partitioning: contiguous chunks
    const V chunk = (n + threads - 1) / threads;
    V s = 0;
    for (int t = 0; t < threads; ++t) {
        const V e = std::min<V>(n, s + chunk);
        if (s >= e) break;
        pool.emplace_back(worker, s, e);
        s = e;
    }
    for (auto& th : pool) th.join();

    return adj;
}

//------------------------------------------------------------------------------
// CLI entry point
//------------------------------------------------------------------------------
int main(int argc, const char** argv)
{
    OptionsParser op;
    auto* help_opt  = op.add_option(false, false, "help",          'h', "",   "Print help and exit");
    auto* in_opt    = op.add_option(true,  true,  "input",         'i', "",   "Input hypergraph basename");
    auto* out_opt   = op.add_option(true,  true,  "output",        'o', "",   "Output graph basename");
    auto* maxe_opt  = op.add_option(false, true,  "max-edge-size", 'm', "",   "Consider only hyperedges with size <= M");
    auto* thr_opt   = op.add_option(false, true,  "threads",       'j', "1",  "Number of threads (default: 1)");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Missing required options\n";
        return EXIT_FAILURE;
    }

    const std::string in_base  = in_opt->get_value();
    const std::string out_base = out_opt->get_value();
    int threads                = std::stoi(thr_opt->get_value());
    const bool use_max         = maxe_opt->is_found();
    const std::uint32_t max_m  = use_max ? static_cast<std::uint32_t>(std::stoul(maxe_opt->get_value())) : 0u;

    try {
        // Load hypergraph and build adjacency (serial or parallel)
        Hypergraph H(in_base);
        std::vector<std::vector<V>> adj =
            (threads <= 1) ? build_gaifman_serial(H, max_m)
                           : build_gaifman_threads(H, max_m, threads);

        // Emit binary graph files
        write_graph_bin(out_base, adj);

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}