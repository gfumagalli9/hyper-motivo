// MIT License
//
// dedup_hypergraph.cpp
// --------------------
// Deduplicate hypergraph vertices by identical incident-hyperedge sets.
//
// Two original vertices u and v are merged iff the sorted list of incident
// hyperedge IDs is exactly the same for both. Each original hyperedge is
// then rewritten by replacing its vertices with the new class IDs and
// removing duplicates inside the edge.
//
// Output is a standard Motivo-format hypergraph written under <output-basename>:
//   <out>.hmeta  : [num_vertices][num_edges]
//   <out>.hef    : offsets for hyperedge->vertex adjacency (size: num_edges+1)
//   <out>.hvd    : concatenated vertex IDs for all hyperedges
//   <out>.vhef   : offsets for vertex->hyperedge incidence (size: num_verts+1)
//   <out>.vhed   : concatenated hyperedge IDs for all vertices
//
// Usage:
//   dedup_hypergraph <input-basename> <output-basename>
//
// Notes:
//   * The number of edges is preserved; the number of vertices may decrease.
//   * If an entire original edge maps to multiple identical class IDs, the
//     result edge is de-duplicated (keeps one copy per class ID).
//   * Empty edges cannot arise (at worst they become size 1 if all vertices
//     in the edge collapse to the same class).
//
// Build (example):
//   g++ -std=c++17 -O2 -I../common -o dedup_hypergraph dedup_hypergraph.cpp

#include "../common/graph/Hypergraph.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Write a hypergraph (given as explicit edge list + inferred vertex count)
// in the Motivo binary layout.
static void write_hypergraph(
    const std::string& basename,
    const std::vector<std::vector<Hypergraph::vertex_t>>& hyperedges)
{
    using vertex_t = Hypergraph::vertex_t;
    using edge_t   = Hypergraph::edge_t;

    // Infer num_verts = max vertex id + 1 (only vertices appearing in edges count)
    vertex_t max_v = 0;
    for (const auto& he : hyperedges)
        for (vertex_t v : he) max_v = std::max(max_v, v);
    const vertex_t num_verts = max_v + 1;
    const edge_t   num_edges = static_cast<edge_t>(hyperedges.size());

    // Build vertex -> incident edges (unsorted initially).
    std::vector<std::vector<edge_t>> v2e(num_verts);
    for (edge_t e = 0; e < num_edges; ++e)
        for (vertex_t v : hyperedges[e]) v2e[v].push_back(e);

    // HEF/HVD: hyperedge -> vertices (CSR-like)
    std::vector<std::uint32_t> offsets_he(num_edges + 1);
    std::vector<vertex_t>      hvd;
    std::uint32_t off = 0;
    hvd.reserve( // reserve a rough upper bound
        [&]{
            std::size_t tot = 0;
            for (const auto& he : hyperedges) tot += he.size();
            return tot;
        }()
    );
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<std::uint32_t>(hyperedges[e].size());
        for (vertex_t v : hyperedges[e]) hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // VHEF/VHED: vertex -> hyperedges (sorted for determinism)
    std::vector<std::uint32_t> offsets_vh(num_verts + 1);
    std::vector<edge_t>        vhed;
    off = 0;
    {
        std::size_t tot = 0;
        for (const auto& inc : v2e) tot += inc.size();
        vhed.reserve(tot);
    }
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto& inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<std::uint32_t>(inc.size());
        for (edge_t e : inc) vhed.push_back(e);
    }
    offsets_vh[num_verts] = off;

    // Emit files with basic error checking.
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open <out>.hmeta for writing");
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open <out>.hef for writing");
        f.write(reinterpret_cast<const char*>(offsets_he.data()),
                static_cast<std::streamsize>(offsets_he.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open <out>.hvd for writing");
        f.write(reinterpret_cast<const char*>(hvd.data()),
                static_cast<std::streamsize>(hvd.size() * sizeof(vertex_t)));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open <out>.vhef for writing");
        f.write(reinterpret_cast<const char*>(offsets_vh.data()),
                static_cast<std::streamsize>(offsets_vh.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open <out>.vhed for writing");
        f.write(reinterpret_cast<const char*>(vhed.data()),
                static_cast<std::streamsize>(vhed.size() * sizeof(edge_t)));
    }
}

int main(int argc, char** argv)
{
    using vertex_t = Hypergraph::vertex_t;
    using edge_t   = Hypergraph::edge_t;

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input-basename> <output-basename>\n";
        return EXIT_FAILURE;
    }
    const std::string in  = argv[1];
    const std::string out = argv[2];

    try {
        // 1) Load input hypergraph
        Hypergraph H(in);
        const vertex_t V = H.number_of_vertices();
        const edge_t   E = H.number_of_hyperedges();

        // 2) Build a canonical "signature" for each vertex:
        //    the sorted list of incident hyperedge IDs.
        //    Vertices with identical signatures are merged into the same class.
        //
        //    We use std::map<vector<edge_t>, vertex_t> so the key is ordered
        //    and allows lookup by value equality of the whole vector.
        std::map<std::vector<edge_t>, vertex_t> signature_to_class;
        std::vector<vertex_t> class_of(V);
        vertex_t next_class = 0;

        for (vertex_t v = 0; v < V; ++v) {
            const std::size_t deg = H.vertex_degree(v);
            std::vector<edge_t> inc(deg);
            for (std::size_t i = 0; i < deg; ++i)
                inc[i] = H.incident_hyperedge(v, static_cast<std::uint32_t>(i));
            std::sort(inc.begin(), inc.end());

            auto [it, inserted] = signature_to_class.emplace(inc, next_class);
            if (inserted) {
                class_of[v] = next_class++;
            } else {
                class_of[v] = it->second;
            }
        }

        // 3) Rebuild each hyperedge by remapping its vertices to class IDs,
        //    then sort+unique to drop duplicates within the edge.
        std::vector<std::vector<vertex_t>> new_edges(E);
        for (edge_t e = 0; e < E; ++e) {
            const std::size_t sz = H.hyperedge_size(e);
            std::vector<vertex_t> buf(sz);
            for (std::size_t i = 0; i < sz; ++i) {
                const vertex_t v = H.hyperedge_vertex(e, static_cast<std::uint32_t>(i));
                buf[i] = class_of[v];
            }
            std::sort(buf.begin(), buf.end());
            buf.erase(std::unique(buf.begin(), buf.end()), buf.end());
            new_edges[e] = std::move(buf);
        }

        // 4) Write deduplicated hypergraph
        write_hypergraph(out, new_edges);
        std::cout << "Wrote deduplicated hypergraph with "
                  << next_class << " vertices and " << E << " edges\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}