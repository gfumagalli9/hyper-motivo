// MIT License
//
// Gaifman graph construction tool: from a hypergraph to an undirected simple graph.
// (omissis banner)

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
#include <iterator>  // upper_bound

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"
#include "../common/io/PairIO.h"       // load_pairs_set
#include "../common/types/PairSet.h"   // CSR pair set

using V = Hypergraph::vertex_t;
using E = Hypergraph::edge_t;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static V safe_cast_V(std::uint64_t x, const char* what)
{
    if (x > static_cast<std::uint64_t>(std::numeric_limits<V>::max())) {
        throw std::runtime_error(std::string(what) +
                                 " exceeds the representable range for vertex_t; "
                                 "use a smaller --max-edge-size or rebuild with a 64-bit format.");
    }
    return static_cast<V>(x);
}

static double bytes_to_MiB(std::uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }
static double bytes_to_MB (std::uint64_t bytes) { return static_cast<double>(bytes) / 1'000'000.0; }

static std::uint64_t sum_directed_entries(const std::vector<std::vector<V>>& adj)
{
    std::uint64_t s = 0;
    for (const auto& nb : adj) s += static_cast<std::uint64_t>(nb.size());
    return s;
}

//------------------------------------------------------------------------------
// Filtraggio ottimizzato con PairSet
//------------------------------------------------------------------------------
//
// Strategia per un vertice u con vicini nb (ordinati, unici):
//  - Prefisso nb[:mid] con v < u: per ciascun v, controlliamo se (v,u) è in pairs.
//    Usiamo un range check su row(v) (se vuota o u fuori [min,max] -> keep),
//    altrimenti binary_search su quella riga.
//  - Suffisso nb[mid:] con v > u: merge lineare contro row(u) per scartare v presenti.
//
template <class Sink, class Vec>
static inline void write_neighbors_filtered_optimized(
    uint32_t u,
    const Vec& nbrs,                // std::vector<V> o simile, ordinato/unique
    const PairSet* exclude_pairs,
    Sink sink)
{
    if (!exclude_pairs || exclude_pairs->empty()) {
        for (auto vv : nbrs) sink(static_cast<uint32_t>(vv));
        return;
    }

    // split index: primo elemento > u
    const uint32_t u32 = u;
    auto it_mid = std::upper_bound(nbrs.begin(), nbrs.end(), static_cast<V>(u32));
    const std::size_t mid = static_cast<std::size_t>(it_mid - nbrs.begin());

    // --- 1) v < u : controlla su row(v) se contiene u
    for (std::size_t i = 0; i < mid; ++i) {
        const uint32_t v = static_cast<uint32_t>(nbrs[i]);

        // riga CSR per v (solo elementi > v)
        const auto degv = exclude_pairs->degree(v);
        if (degv == 0) { sink(v); continue; }

        const auto [rp, rl] = exclude_pairs->row(v);
        // range check: se u fuori [rp[0], rp[rl-1]] -> sicuramente assente
        if (rp[0] > u32 || rp[rl - 1] < u32) { sink(v); continue; }

        // cerca u in row(v)
        const bool found = std::binary_search(rp, rp + rl, u32);
        if (!found) sink(v);
    }

    // --- 2) v > u : merge lineare tra nbrs[mid..] e row(u)
    const auto [ru, lu] = exclude_pairs->row(u32); // ru: v>u da escludere
    std::size_t j = 0; // indice in ru

    for (std::size_t i = mid; i < nbrs.size(); ++i) {
        const uint32_t v = static_cast<uint32_t>(nbrs[i]);
        // avanza in ru fino a >= v
        while (j < lu && ru[j] < v) ++j;
        if (j == lu || ru[j] != v) {
            sink(v); // tieni v se non escluso
        }
    }
}

//------------------------------------------------------------------------------
// Binary writers (.gof / .gof64) con filtraggio ottimizzato
//------------------------------------------------------------------------------

static void write_graph_bin32(const std::string& out_base,
                              const std::vector<std::vector<V>>& adj,
                              const PairSet* exclude_pairs)
{
    const V n = static_cast<V>(adj.size());

    std::ofstream gof(out_base + ".gof", std::ios::binary);
    std::ofstream ged(out_base + ".ged", std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header: [n:uint32][E:uint32=0]
    gof.write(reinterpret_cast<const char*>(&n), sizeof(V));
    std::uint32_t placeholder_edges = 0;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint32_t));

    std::uint32_t written = 0;
    std::uint64_t directed_written = 0ULL;

    for (V u = 0; u < n; ++u) {
        const std::uint32_t off = written;
        gof.write(reinterpret_cast<const char*>(&off), sizeof(std::uint32_t));

        const auto& nb = adj[u];
        if (!nb.empty()) {
            write_neighbors_filtered_optimized(
                static_cast<uint32_t>(u), nb, exclude_pairs,
                [&](uint32_t v){
                    ged.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
                    ++written; ++directed_written;
                }
            );
        }
    }
    // sentinel
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));

    // patch E
    const std::uint64_t undirected = directed_written / 2ULL;
    if (undirected > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("directed entries pushed E beyond 32-bit range; use .gof64");
    const std::uint32_t E32 = static_cast<std::uint32_t>(undirected);
    gof.seekp(sizeof(V), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&E32), sizeof(std::uint32_t));
}

static void write_graph_bin64(const std::string& out_base,
                              const std::vector<std::vector<V>>& adj,
                              const PairSet* exclude_pairs)
{
    const V n = static_cast<V>(adj.size());

    std::ofstream gof(out_base + ".gof64", std::ios::binary);
    std::ofstream ged(out_base + ".ged",   std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof64");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    // Header: [n:uint32][E:uint64=0]
    const std::uint32_t n32 = static_cast<std::uint32_t>(n);
    gof.write(reinterpret_cast<const char*>(&n32), sizeof(std::uint32_t));
    std::uint64_t placeholder_edges = 0ULL;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint64_t));

    std::uint64_t written = 0ULL;
    std::uint64_t directed_written = 0ULL;

    for (V u = 0; u < n; ++u) {
        const std::uint64_t off = written;
        gof.write(reinterpret_cast<const char*>(&off), sizeof(std::uint64_t));

        const auto& nb = adj[u];
        if (!nb.empty()) {
            write_neighbors_filtered_optimized(
                static_cast<uint32_t>(u), nb, exclude_pairs,
                [&](uint32_t v){
                    ged.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
                    ++written; ++directed_written;
                }
            );
        }
    }
    // sentinel
    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));

    // patch E
    const std::uint64_t undirected = directed_written / 2ULL;
    gof.seekp(sizeof(std::uint32_t), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&undirected), sizeof(std::uint64_t));
}

//------------------------------------------------------------------------------
// Costruzione Gaifman (seriale / threads) — invariata
//------------------------------------------------------------------------------

static std::vector<std::vector<V>>
build_gaifman_serial(const Hypergraph& H, std::uint32_t max_m /* 0 = disabled */)
{
    const V n = H.number_of_vertices();
    std::vector<std::vector<V>> adj(n);

    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

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
    }
    return adj;
}

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
// Streaming serial (.gof/.gof64) con filtraggio ottimizzato
//------------------------------------------------------------------------------

static void build_gaifman_streaming_serial32(const Hypergraph& H,
                                             const std::string& out_base,
                                             std::uint32_t max_m,
                                             const PairSet* exclude_pairs)
{
    const V n = H.number_of_vertices();

    std::ofstream gof(out_base + ".gof", std::ios::binary);
    std::ofstream ged(out_base + ".ged", std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    gof.write(reinterpret_cast<const char*>(&n), sizeof(V));
    std::uint32_t placeholder_edges = 0;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint32_t));

    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    std::uint64_t directed_written = 0ULL;
    std::uint32_t written = 0;

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

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

        gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));

        if (!nb.empty()) {
            write_neighbors_filtered_optimized(
                static_cast<uint32_t>(u), nb, exclude_pairs,
                [&](uint32_t v){
                    ged.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
                    ++written; ++directed_written;
                }
            );
        }
    }

    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint32_t));

    const std::uint64_t undirected64 = directed_written / 2ull;
    if (undirected64 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("num_undirected_edges exceeds 32-bit during finalize (unexpected)");
    const std::uint32_t E32 = static_cast<std::uint32_t>(undirected64);
    gof.seekp(sizeof(V), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&E32), sizeof(std::uint32_t));
}

static void build_gaifman_streaming_serial64(const Hypergraph& H,
                                             const std::string& out_base,
                                             std::uint32_t max_m,
                                             const PairSet* exclude_pairs)
{
    const V n = H.number_of_vertices();

    std::ofstream gof(out_base + ".gof64", std::ios::binary);
    std::ofstream ged(out_base + ".ged",   std::ios::binary);
    if (!gof) throw std::runtime_error("Cannot open " + out_base + ".gof64");
    if (!ged) throw std::runtime_error("Cannot open " + out_base + ".ged");

    const std::uint32_t n32 = static_cast<std::uint32_t>(n);
    gof.write(reinterpret_cast<const char*>(&n32), sizeof(std::uint32_t));
    std::uint64_t placeholder_edges = 0;
    gof.write(reinterpret_cast<const char*>(&placeholder_edges), sizeof(std::uint64_t));

    std::vector<std::uint32_t> seen(n, 0);
    std::uint32_t token = 1;

    std::uint64_t written = 0; // directed entries

    for (V u = 0; u < n; ++u, ++token) {
        if (token == 0) { std::fill(seen.begin(), seen.end(), 0); token = 1; }

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

        gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));

        if (!nb.empty()) {
            write_neighbors_filtered_optimized(
                static_cast<uint32_t>(u), nb, exclude_pairs,
                [&](uint32_t v){
                    ged.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
                    ++written;
                }
            );
        }
    }

    gof.write(reinterpret_cast<const char*>(&written), sizeof(std::uint64_t));

    const std::uint64_t undirected = written / 2ull;
    gof.seekp(sizeof(std::uint32_t), std::ios::beg);
    gof.write(reinterpret_cast<const char*>(&undirected), sizeof(std::uint64_t));
}

//------------------------------------------------------------------------------
// Estimatori (come prima)
//------------------------------------------------------------------------------

static std::tuple<V, std::uint64_t, std::uint64_t>
estimate_upper_bound(const Hypergraph& H, std::uint32_t max_m)
{
    const V n = H.number_of_vertices();
    const E m = H.number_of_hyperedges();

    long double undirected_lb = 0.0L;
    for (E e = 0; e < m; ++e) {
        const std::uint32_t sz = H.hyperedge_size(e);
        if (max_m && sz > max_m) continue;
        if (sz >= 2) undirected_lb += (static_cast<long double>(sz) * (sz - 1)) / 2.0L;
    }
    const std::uint64_t ub_undirected = static_cast<std::uint64_t>(undirected_lb + 0.5L);
    const std::uint64_t ub_directed   = ub_undirected * 2ULL;
    return {n, ub_undirected, ub_directed};
}

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
        std::sort(nb.begin(), nb.end());
        directed_sum += static_cast<std::uint64_t>(nb.size());
    }

    const std::uint64_t undirected = directed_sum / 2ULL;
    return {n, undirected, directed_sum};
}

//------------------------------------------------------------------------------
// CLI
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
    auto* excl_opt   = op.add_option(false, true,  "exclude-pairs",   'X', "",   "Canonical .pairs file (u<v) whose edges must be removed");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const bool do_stream   = stream_opt->is_found();
    const bool do_est_only = est_only->is_found();
    const bool do_est_up   = est_upper->is_found();

    if (!in_opt->is_found()) { std::cerr << "Missing required --input\n"; return EXIT_FAILURE; }
    if (!out_opt->is_found() && !(do_est_only || do_est_up)) {
        std::cerr << "Missing required --output (not needed with --estimate-only/--estimate-upper)\n";
        return EXIT_FAILURE;
    }

    const std::string in_base  = in_opt->get_value();
    const std::string out_base = out_opt->get_value();
    int threads                = std::stoi(thr_opt->get_value());
    const bool use_max         = maxe_opt->is_found();
    const std::uint32_t max_m  = use_max ? static_cast<std::uint32_t>(std::stoul(maxe_opt->get_value())) : 0u;

    // Carica exclude-pairs se richiesto
    PairSet exclude_pairs;
    const PairSet* exclude_ptr = nullptr;
    if (excl_opt->is_found()) {
        const std::string pth = excl_opt->get_value();
        try {
            exclude_pairs = load_pairs_set(pth);
            exclude_ptr = &exclude_pairs;
            std::cout << "[filter] exclude-pairs loaded: M=" << exclude_pairs.size()
                      << ", n~=" << exclude_pairs.n() << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "Warning: failed to load --exclude-pairs '" << pth << "': " << ex.what() << "\n";
        }
    }

    try {
        Hypergraph H(in_base);

        // Modalità stima
        if (do_est_up) {
            auto [n, ub_undirected, ub_directed] = estimate_upper_bound(H, max_m);
            const std::size_t id_bytes = sizeof(V);
            const std::uint64_t ged_bytes = ub_directed * static_cast<std::uint64_t>(id_bytes);
            const std::uint64_t gof_bytes_32 = (static_cast<std::uint64_t>(n) + 1ULL) * 4ULL + 8ULL;
            const std::uint64_t gof_bytes_64 = (static_cast<std::uint64_t>(n) + 1ULL) * 8ULL + 12ULL;

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

        // Build con selezione formato automatica
        if (do_stream) {
            if (thr_opt->is_found() && threads != 1) {
                std::cerr << "Warning: --threads is ignored in streaming mode (using serial build).\n";
            }
            auto [n, ub_undirected, ub_directed] = estimate_upper_bound(H, max_m);
            const bool need_wide = (ub_directed > std::numeric_limits<std::uint32_t>::max());
            std::cout << "[stream] directed entries (upper bound) = " << ub_directed
                      << " -> writing " << (need_wide ? ".gof64" : ".gof") << "\n";

            if (need_wide) build_gaifman_streaming_serial64(H, out_base, max_m, exclude_ptr);
            else           build_gaifman_streaming_serial32(H, out_base, max_m, exclude_ptr);

        } else {
            std::vector<std::vector<V>> adj =
                (threads <= 1) ? build_gaifman_serial(H, max_m)
                               : build_gaifman_threads(H, max_m, threads);

            const std::uint64_t directed_pre = sum_directed_entries(adj);
            const bool need_wide = (directed_pre > std::numeric_limits<std::uint32_t>::max());
            std::cout << "[in-memory] directed entries (pre-filter) = " << directed_pre
                      << " -> writing " << (need_wide ? ".gof64" : ".gof") << "\n";

            if (need_wide) write_graph_bin64(out_base, adj, exclude_ptr);
            else           write_graph_bin32(out_base, adj, exclude_ptr);
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}