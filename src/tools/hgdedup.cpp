// dedup_hypergraph.cpp
// Build with: g++ -std=c++17 -O2 -I/path/to/common -o dedup_hypergraph dedup_hypergraph.cpp

#include "../common/graph/Hypergraph.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <fstream>

// Scrive il risultato usando lo stesso formato binario dei vostri tool
static void write_hypergraph(
    const std::string &basename,
    const std::vector<std::vector<Hypergraph::vertex_t>> &hyperedges
) {
    using vertex_t = Hypergraph::vertex_t;
    using edge_t   = Hypergraph::edge_t;

    // determino num_verts = max vertice +1
    vertex_t max_v = 0;
    for (auto &he : hyperedges)
        for (auto v : he)
            max_v = std::max(max_v, v);
    vertex_t num_verts = max_v + 1;
    edge_t   num_edges = static_cast<edge_t>(hyperedges.size());

    // invert mapping v->edge list
    std::vector<std::vector<edge_t>> v2e(num_verts);
    for (edge_t e = 0; e < num_edges; ++e)
        for (auto v : hyperedges[e])
            v2e[v].push_back(e);

    // offsets_he + hvd
    std::vector<uint32_t> offsets_he(num_edges+1);
    std::vector<vertex_t> hvd;
    uint32_t off = 0;
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<uint32_t>(hyperedges[e].size());
        for (auto v : hyperedges[e]) hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // offsets_vh + vhed
    std::vector<uint32_t> offsets_vh(num_verts+1);
    std::vector<edge_t>   vhed;
    off = 0;
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto &inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<uint32_t>(inc.size());
        for (auto e : inc) vhed.push_back(e);
    }
    offsets_vh[num_verts] = off;

    // scrivo file
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary);
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary);
        f.write(reinterpret_cast<const char*>(offsets_he.data()), offsets_he.size()*sizeof(uint32_t));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary);
        f.write(reinterpret_cast<const char*>(hvd.data()), hvd.size()*sizeof(vertex_t));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary);
        f.write(reinterpret_cast<const char*>(offsets_vh.data()), offsets_vh.size()*sizeof(uint32_t));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary);
        f.write(reinterpret_cast<const char*>(vhed.data()), vhed.size()*sizeof(edge_t));
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input-basename> <output-basename>\n";
        return 1;
    }
    std::string in  = argv[1];
    std::string out = argv[2];

    // 1) carico
    Hypergraph H(in);
    size_t  V = H.number_of_vertices();
    size_t  E = H.number_of_hyperedges();

    // 2) per ogni vertice raccolgo il suo set di hyperedges e creo una chiave (vector<edge_t>)
    std::map<std::vector<Hypergraph::edge_t>, Hypergraph::vertex_t> repr;
    std::vector<Hypergraph::vertex_t> type_id(V);
    Hypergraph::edge_t new_v_counter = 0;

    for (Hypergraph::vertex_t v = 0; v < V; ++v) {
        size_t deg = H.vertex_degree(v);
        std::vector<Hypergraph::edge_t> inc(deg);
        for (size_t i = 0; i < deg; ++i)
            inc[i] = H.incident_hyperedge(v, i);
        std::sort(inc.begin(), inc.end());
        auto it = repr.find(inc);
        if (it == repr.end()) {
            repr.emplace(inc, new_v_counter);
            type_id[v] = new_v_counter++;
        } else {
            type_id[v] = it->second;
        }
    }

    // 3) ricostruisco le hyperedges: per ogni e, elenco i vertici originali, li mappo su type_id[v], rimuovo duplicati
    std::vector<std::vector<Hypergraph::vertex_t>> new_he(E);
    for (Hypergraph::edge_t e = 0; e < E; ++e) {
        size_t sz = H.hyperedge_size(e);
        std::vector<Hypergraph::vertex_t> buf(sz);
        for (size_t i = 0; i < sz; ++i) {
            auto v = H.hyperedge_vertex(e, i);
            buf[i] = type_id[v];
        }
        std::sort(buf.begin(), buf.end());
        buf.erase(std::unique(buf.begin(), buf.end()), buf.end());
        new_he[e] = std::move(buf);
    }

    // 4) scrivo il nuovo ipergrafo
    write_hypergraph(out, new_he);
    std::cout << "Wrote deduplicated hypergraph with " << new_v_counter << " vertices and " << E << " edges\n";
    return 0;
}