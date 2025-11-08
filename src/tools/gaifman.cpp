// MIT License
//
// Gaifman graph construction tool: from a hypergraph to an undirected simple graph.
// - Each hyperedge induces a clique among its vertices.
// - Optional filter: only hyperedges with size <= M (useful to build a Gaifman-LOW).
// - Parallel build with std::thread (no OpenMP).
// - NEW: Streaming serial build (low memory) and size estimators.
// - NEW: Auto-selects .gof (32-bit offsets) or .gof64 (64-bit offsets) when needed.
//
// Output binary graph formats (compatible with updated UndirectedGraph):
//   out_base.gof   : [uint32 n][uint32 E][offsets[0..n] as uint32]
//   out_base.gof64 : [uint32 n][uint64 E][offsets[0..n] as uint64]
//   out_base.ged   : neighbors as uint32 vertex IDs, concatenated.
// For both formats, offsets[i] is the starting index of vertex i's adjacency in .ged
// and the last sentinel offsets[n] equals the total number of stored neighbors.
//
// CLI summary:
//   --input,  -i             : input hypergraph basename
//   --output, -o             : output graph basename
//   --max-edge-size, -m M    : consider only hyperedges with size <= M (0 disables filter)
//   --threads, -j            : number of threads (default: 1) [used only in non-streaming mode]
//   --stream,  -s            : enable streaming serial build (low memory; ignores --threads)
//   --estimate-only,  -E     : exact degree-sum estimation (dedup-aware serial scan), no files written
//   --estimate-upper, -U     : upper-bound estimation via clique expansion, no files written
//
// Examples:
//   # Original behavior (in-memory; parallel allowed):
//   gaifman -i data/hg -o data/hg.gaifman -j 8 -m 128
//
//   # Streaming serial build (low memory):
//   gaifman -i data/hg -o data/hg_low.gaifman -s -m 128
//
//   # Estimations:
//   gaifman -i data/hg -U -m 128   # fast upper bound
//   gaifman -i data/hg -E -m 128   # exact directed entries via serial dedup pass
//

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>   // pretty MiB/MB printing
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"

using V = Hypergraph::vertex_t;
using E = Hypergraph::edge_t;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

// Safe cast helper: ensure x fits into vertex_t (V). Throws on overflow.
static V safe_cast_V(std::uint64_t x, const char* what)
{
    if (x > static_cast<std::uint64_t>(std::numeric_limits<V>::max())) {
        throw std::runtime_error(std::string(what) +
                                 " exceeds the representable range for vertex_t; "
                                 "use a smaller --max-edge-size or rebuild with a 64-bit format.");
    }
    return static_cast<V>(x);
}

// Byte-size converters for reporting (MiB = 2^20, MB = 10^6).
static double bytes_to_MiB(std::uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }
static double bytes_to_MB (std::uint64_t bytes) { return static_cast<double>(bytes) / 1'000'000.0; }

// Sum of directed entries from an in-memory adjacency.
static std::uint64_t sum_directed_entries(const std::vector<std::vector<V>>& adj)
{
    std::uint64_t s = 0;
    for (const auto& nb : adj) s += static_cast<std::uint64_t>(nb.size());
    return s;
}

//------------------------------------------------------------------------------
// Binary writers (.gof / .gof64)
// - adj[u] must be sorted and duplicate-free.
// - undirected edge count is inferred as sum(deg)/2.
// - .ged always stores uint32 vertex IDs.
//------------------------------------------------------------------------------

static void write_graph_bin32(const std::string& out_base,
                              const std::vector<std::vector<V>>& adj)
{
    const V n = static_cast<V>(adj.size());
    const std::uint64_t directed = sum_directed_entries(adj);
    if (directed > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("directed entries exceed 32-bit range; write_graph_bin32 not applicable");
    const std::uint32_t undirected_edges = static_cast<std::uint32_t>(directed / 2ull);

    std::ofstream gof(out_base + ".gof", std::ios::binary);
    std::ofstream ged(out_base + ".ged", std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header: [n:uint32][E:uint32]
    gof.write(reinterpret_cast<const char*>(&n), sizeof(V));
    gof.write(reinterpret_cast<const char*>(&undirected_edges), sizeof(std::uint32_t));

    // Offsets + neighbors payload
    std::uint32_t written = 0;
    for (V u = 0; u < n; ++u) {
        const std::uint32_t off = written;
        gof.write(reinterpret_cast<const char*>(&off), sizeof(std::uint32_t));

        if (!adj[u].empty()) {
            ged.write(reinterpret_cast<const char*>(adj[u].data()),
                      static_cast<std::streamsize>(adj[u].size() * sizeof(V)));
            written += static_cast<std::uint32_t>(adj[u].size());
        }
    }
    // Final sentinel offset (offsets[n])
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));
}

static void write_graph_bin64(const std::string& out_base,
                              const std::vector<std::vector<V>>& adj)
{
    const V n = static_cast<V>(adj.size());
    const std::uint64_t directed = sum_directed_entries(adj);
    const std::uint64_t undirected_edges = directed / 2ull;

    std::ofstream gof(out_base + ".gof64", std::ios::binary);
    std::ofstream ged(out_base + ".ged",   std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof64");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header: [n:uint32][E:uint64]
    const std::uint32_t n32 = static_cast<std::uint32_t>(n);
    gof.write(reinterpret_cast<const char*>(&n32), sizeof(std::uint32_t));
    gof.write(reinterpret_cast<const char*>(&undirected_edges), sizeof(std::uint64_t));

    // Offsets + neighbors payload (uint64 offsets)
    std::uint64_t written = 0;
    for (V u = 0; u < n; ++u) {
        const std::uint64_t off = written;
        gof.write(reinterpret_cast<const char*>(&off), sizeof(std::uint64_t));

        if (!adj[u].empty()) {
            ged.write(reinterpret_cast<const char*>(adj[u].data()),
                      static_cast<std::streamsize>(adj[u].size() * sizeof(V)));
            written += static_cast<std::uint64_t>(adj[u].size());
        }
    }
    // Final sentinel offset (offsets[n])
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));
}

//------------------------------------------------------------------------------
// Serial Gaifman construction (in-memory).
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
            adj[u] = std::move(nb);
        }
    };

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
// Streaming serial Gaifman construction (.gof, 32-bit offsets).
//------------------------------------------------------------------------------
static void build_gaifman_streaming_serial32(const Hypergraph& H,
                                             const std::string& out_base,
                                             std::uint32_t max_m)
{
    const V n = H.number_of_vertices();

    std::ofstream gof(out_base + ".gof", std::ios::binary);
    std::ofstream ged(out_base + ".ged", std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header with placeholders: [n:uint32][E:uint32=0], then offsets[0]
    gof.write(reinterpret_cast<const char*>(&n), sizeof(V));
    std::uint32_t placeholder_edges = 0;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint32_t));
    //std::uint32_t off0 = 0;
    //gof.write(reinterpret_cast<const char*>(&off0), sizeof(std::uint32_t));

    // Per-vertex dedup state
    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    std::uint64_t written_directed64 = 0; // track in 64-bit for safety
    std::uint32_t written = 0;

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

        // Capacity hint
        std::uint64_t cap = 0;
        const std::uint32_t deg = H.vertex_degree(u);
        for (std::uint32_t i = 0; i < deg; ++i) {
            const E e = H.incident_hyperedge(u, i);
            const std::uint32_t sz = H.hyperedge_size(e);
            if (max_m && sz > max_m) continue;
            cap += (sz > 0 ? (sz - 1) : 0);
        }

        std::vector<V> nb;
        if (cap > 0) nb.reserve(static_cast<size_t>(cap));

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

        // offsets[u] (uint32)
        gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));

        if (!nb.empty()) {
            ged.write(reinterpret_cast<const char*>(nb.data()),
                      static_cast<std::streamsize>(nb.size() * sizeof(V)));
            written += static_cast<std::uint32_t>(nb.size());
            written_directed64 += static_cast<std::uint64_t>(nb.size());
        }
    }

    // Final sentinel offsets[n] (uint32)
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));

    // Patch num_undirected_edges at the header
    const std::uint64_t undirected64 = written_directed64 / 2ull;
    if (undirected64 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("num_undirected_edges exceeds 32-bit during finalize (unexpected)");
    const std::uint32_t E32 = static_cast<std::uint32_t>(undirected64);
    gof.seekp(sizeof(V), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&E32), sizeof(std::uint32_t));
}

//------------------------------------------------------------------------------
// Streaming serial Gaifman construction (.gof64, 64-bit offsets).
//------------------------------------------------------------------------------
static void build_gaifman_streaming_serial64(const Hypergraph& H,
                                             const std::string& out_base,
                                             std::uint32_t max_m)
{
    const V n = H.number_of_vertices();

    std::ofstream gof(out_base + ".gof64", std::ios::binary);
    std::ofstream ged(out_base + ".ged",   std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof64");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header with placeholders: [n:uint32][E:uint64=0], then offsets[0]
    const std::uint32_t n32 = static_cast<std::uint32_t>(n);
    gof.write(reinterpret_cast<const char*>(&n32), sizeof(std::uint32_t));
    std::uint64_t placeholder_edges = 0;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint64_t));
    //std::uint64_t off0 = 0;
    //gof.write(reinterpret_cast<const char*>(&off0), sizeof(std::uint64_t));

    // Per-vertex dedup state
    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    std::uint64_t written = 0; // 64-bit offsets

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

        // Capacity hint
        std::uint64_t cap = 0;
        const std::uint32_t deg = H.vertex_degree(u);
        for (std::uint32_t i = 0; i < deg; ++i) {
            const E e = H.incident_hyperedge(u, i);
            const std::uint32_t sz = H.hyperedge_size(e);
            if (max_m && sz > max_m) continue;
            cap += (sz > 0 ? (sz - 1) : 0);
        }

        std::vector<V> nb;
        if (cap > 0) nb.reserve(static_cast<size_t>(cap));

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

        // offsets[u] (uint64)
        gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));

        if (!nb.empty()) {
            ged.write(reinterpret_cast<const char*>(nb.data()),
                      static_cast<std::streamsize>(nb.size() * sizeof(V)));
            written += static_cast<std::uint64_t>(nb.size());
        }
    }

    // Final sentinel offsets[n] (uint64)
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));

    // Patch num_undirected_edges at the header (uint64)
    const std::uint64_t undirected = written / 2ull;
    gof.seekp(sizeof(std::uint32_t), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&undirected), sizeof(std::uint64_t));
}

//------------------------------------------------------------------------------
// Estimators
//------------------------------------------------------------------------------

// Upper bound estimate (clique expansion, ignores duplicates across hyperedges).
static std::tuple<V, std::uint64_t, std::uint64_t>
estimate_upper_bound(const Hypergraph& H, std::uint32_t max_m)
{
    const V n = H.number_of_vertices();
    const E m = H.number_of_hyperedges();

    long double undirected_lb = 0.0L;
    for (E e = 0; e < m; ++e) {
        const std::uint32_t sz = H.hyperedge_size(e);
        if (max_m && sz > max_m) continue;
        if (sz >= 2) {
            undirected_lb += (static_cast<long double>(sz) * (sz - 1)) / 2.0L;
        }
    }
    const std::uint64_t ub_undirected = static_cast<std::uint64_t>(undirected_lb + 0.5L);
    const std::uint64_t ub_directed   = ub_undirected * 2ULL;
    return {n, ub_undirected, ub_directed};
}

// Exact directed degree-sum estimation via a serial scan with per-vertex dedup.
static std::tuple<V, std::uint64_t, std::uint64_t>
estimate_exact_degree_sum(const Hypergraph& H, std::uint32_t max_m)
{
    const V n = H.number_of_vertices();
    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    std::uint64_t directed_sum = 0;

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

        std::vector<V> nb;

        // Capacity hint
        std::uint64_t cap = 0;
        const std::uint32_t deg = H.vertex_degree(u);
        for (std::uint32_t i = 0; i < deg; ++i) {
            const E e = H.incident_hyperedge(u, i);
            const std::uint32_t sz = H.hyperedge_size(e);
            if (max_m && sz > max_m) continue;
            cap += (sz > 0 ? (sz - 1) : 0);
        }
        if (cap > 0) nb.reserve(static_cast<size_t>(cap));

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
        directed_sum += static_cast<std::uint64_t>(nb.size());
    }

    const std::uint64_t undirected = directed_sum / 2ULL;
    return {n, undirected, directed_sum};
}

//------------------------------------------------------------------------------
// CLI entry point
//------------------------------------------------------------------------------
int main(int argc, const char** argv)
{
    OptionsParser op;
    auto* help_opt   = op.add_option(false, false, "help",            'h', "",   "Print help and exit");
    auto* in_opt     = op.add_option(false, true,  "input",           'i', "",   "Input hypergraph basename");
    auto* out_opt    = op.add_option(false, true,  "output",          'o', "",   "Output graph basename");
    auto* maxe_opt   = op.add_option(false, true,  "max-edge-size",   'm', "",   "Consider only hyperedges with size <= M");
    auto* thr_opt    = op.add_option(false, true,  "threads",         'j', "1",  "Number of threads (default: 1)");
    auto* stream_opt = op.add_option(false, false, "stream",          's', "",   "Enable streaming serial build (low memory)");
    auto* est_only   = op.add_option(false, false, "estimate-only",   'E', "",   "Exact degree-sum estimation (no output files)");
    auto* est_upper  = op.add_option(false, false, "estimate-upper",  'U', "",   "Upper-bound estimation via clique expansion (no output files)");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const bool do_stream   = stream_opt->is_found();
    const bool do_est_only = est_only->is_found();
    const bool do_est_up   = est_upper->is_found();

    if (!in_opt->is_found()) {
        std::cerr << "Missing required --input\n";
        return EXIT_FAILURE;
    }
    if (!out_opt->is_found() && !(do_est_only || do_est_up)) {
        std::cerr << "Missing required --output (not needed with --estimate-only/--estimate-upper)\n";
        return EXIT_FAILURE;
    }

    const std::string in_base  = in_opt->get_value();
    const std::string out_base = out_opt->get_value();
    int threads                = std::stoi(thr_opt->get_value());
    const bool use_max         = maxe_opt->is_found();
    const std::uint32_t max_m  = use_max ? static_cast<std::uint32_t>(std::stoul(maxe_opt->get_value())) : 0u;

    try {
        Hypergraph H(in_base);

        // Estimation modes (no files written).
        if (do_est_up) {
            auto [n, ub_undirected, ub_directed] = estimate_upper_bound(H, max_m);
            const std::size_t id_bytes = sizeof(V);
            const std::uint64_t ged_bytes = ub_directed * static_cast<std::uint64_t>(id_bytes);
            const std::uint64_t gof_bytes_32 = (static_cast<std::uint64_t>(n) + 1ULL) * 4ULL + 8ULL;   // [n:u32][E:u32] + (n+1) u32
            const std::uint64_t gof_bytes_64 = (static_cast<std::uint64_t>(n) + 1ULL) * 8ULL + 12ULL;  // [n:u32][E:u64] + (n+1) u64

            std::cout << "Upper bound estimate (clique expansion):\n"
                      << "  vertices (n): " << static_cast<std::uint64_t>(n) << "\n"
                      << "  undirected edges (<=): " << ub_undirected << "\n"
                      << "  directed entries (<=): " << ub_directed << "\n"
                      << std::fixed << std::setprecision(2)
                      << "  .ged size (<=): " << bytes_to_MiB(ged_bytes) << " MiB (~" << bytes_to_MB(ged_bytes) << " MB)\n"
                      << "  .gof size 32-bit hdr (<=): " << bytes_to_MiB(gof_bytes_32) << " MiB (~" << bytes_to_MB(gof_bytes_32) << " MB)\n"
                      << "  .gof64 size 64-bit hdr (<=): " << bytes_to_MiB(gof_bytes_64) << " MiB (~" << bytes_to_MB(gof_bytes_64) << " MB)\n";
            return EXIT_SUCCESS;
        }

        if (do_est_only) {
            auto [n, undirected, directed] = estimate_exact_degree_sum(H, max_m);
            const std::size_t id_bytes = sizeof(V);
            const std::uint64_t ged_bytes = directed * static_cast<std::uint64_t>(id_bytes);
            const std::uint64_t gof_bytes_32 = (static_cast<std::uint64_t>(n) + 1ULL) * 4ULL + 8ULL;
            const std::uint64_t gof_bytes_64 = (static_cast<std::uint64_t>(n) + 1ULL) * 8ULL + 12ULL;

            std::cout << "Exact degree-sum estimate (dedup-aware serial scan):\n"
                      << "  vertices (n): " << static_cast<std::uint64_t>(n) << "\n"
                      << "  undirected edges: " << undirected << "\n"
                      << "  directed entries: " << directed << "\n"
                      << std::fixed << std::setprecision(2)
                      << "  .ged size: " << bytes_to_MiB(ged_bytes) << " MiB (~" << bytes_to_MB(ged_bytes) << " MB)\n"
                      << "  .gof size 32-bit hdr: " << bytes_to_MiB(gof_bytes_32) << " MiB (~" << bytes_to_MB(gof_bytes_32) << " MB)\n"
                      << "  .gof64 size 64-bit hdr: " << bytes_to_MiB(gof_bytes_64) << " MiB (~" << bytes_to_MB(gof_bytes_64) << " MB)\n";
            return EXIT_SUCCESS;
        }

        // Build modes with auto .gof / .gof64 selection
        if (do_stream) {
            if (thr_opt->is_found() && threads != 1) {
                std::cerr << "Warning: --threads is ignored in streaming mode (using serial build).\n";
            }
            // Decide format with an exact pre-pass (low memory).
            auto [n, undirected, directed] = estimate_upper_bound(H, max_m);
            const bool need_wide = (directed > std::numeric_limits<std::uint32_t>::max());
            std::cout << "[stream] directed entries = " << directed
                      << " -> writing " << (need_wide ? ".gof64" : ".gof") << "\n";

            if (need_wide) build_gaifman_streaming_serial64(H, out_base, max_m);
            else           build_gaifman_streaming_serial32(H, out_base, max_m);

        } else {
            // Original behavior: in-memory adjacency + (optional) threads.
            std::vector<std::vector<V>> adj =
                (threads <= 1) ? build_gaifman_serial(H, max_m)
                               : build_gaifman_threads(H, max_m, threads);

            const std::uint64_t directed = sum_directed_entries(adj);
            const bool need_wide = (directed > std::numeric_limits<std::uint32_t>::max());
            std::cout << "[in-memory] directed entries = " << directed
                      << " -> writing " << (need_wide ? ".gof64" : ".gof") << "\n";

            if (need_wide) write_graph_bin64(out_base, adj);
            else           write_graph_bin32(out_base, adj);
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}