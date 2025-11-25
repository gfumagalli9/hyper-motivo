// MIT License
//
// See header for a high-level description.

#include "SequentialNWSBuilder.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

SequentialNWSBuilder::SequentialNWSBuilder(const Hypergraph*   H,
                                           const TreeletList*  treelet_list,
                                           const TreeletTable* treelet_table,
                                           std::ostream*       output,
                                           bool                allow_singletons) noexcept
    : H(H)
    , treelet_list(treelet_list)
    , treelet_table(treelet_table)
    , output(output)
    , allow_singletons(allow_singletons)
    , builder(H, treelet_table) // NWSBuilder needs H + the table to read C(T,v)
{
    // Pre-size the map of accumulators (one vector per treelet).
    if (H && treelet_list) {
        nws_counts.reserve(treelet_list->size());
        const auto nv = H->number_of_vertices();
        for (const auto& t : *treelet_list) {
            nws_counts.emplace(t, std::vector<CountT>(nv, static_cast<CountT>(0)));
        }
    }
}

void SequentialNWSBuilder::build()
{
    if (!H || !treelet_list || !treelet_table || !output) {
        throw std::runtime_error("SequentialNWSBuilder: invalid constructor arguments");
    }

    // (1) File header: number of vertices (match other builders’ header type).
    const Hypergraph::vertex_t nv = H->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&nv), sizeof(nv));

    // (2) Seed the BFS with all singleton subtypes: one hyperedge at a time.
    //
    // Invariant for NWSBuilder::build(): st.edges and st.verts must be sorted.
    for (Hypergraph::edge_t he = 0; he < H->number_of_hyperedges(); ++he) {
        const std::uint32_t sz = H->hyperedge_size(he);
        std::vector<Hypergraph::vertex_t> he_verts;
        he_verts.reserve(sz);
        for (std::uint32_t i = 0; i < sz; ++i) {
            he_verts.push_back(H->hyperedge_vertex(he, i));
        }
        std::sort(he_verts.begin(), he_verts.end());
        he_verts.erase(std::unique(he_verts.begin(), he_verts.end()), he_verts.end());

        // weight in the subtype is not used by NWSBuilder::build() for the input “st”,
        // it recomputes the sums it needs; set it to 0 for clarity.
        EdgeSubtype st{/*edges=*/{he}, /*verts=*/std::move(he_verts), /*weight=*/0};

        for (const auto& t : *treelet_list) {
            work_queue.emplace(t, st);
            visited[t].insert(st); // ensure we don’t enqueue the same (t,st) twice
        }
    }

    // (3) BFS over (treelet, subtype).
    while (!work_queue.empty()) {
        auto [cur_treelet, cur_st] = std::move(work_queue.front());
        work_queue.pop();

        // NWS step: update per-vertex signed counts and enumerate one-step extensions.
        auto next = builder.build(cur_st, cur_treelet, nws_counts[cur_treelet]);

        // Enqueue only *new* subtypes for this treelet.
        for (auto& st_next : next) {
            auto& seen = visited[cur_treelet];
            const auto [_, inserted] = seen.insert(st_next);
            if (inserted) {
                work_queue.emplace(cur_treelet, std::move(st_next));
            }
        }
    }

    // (4) For each vertex u, serialize the (treelet,count) pairs.
    //
    // We only emit non-zero counts; NWSBuilder::to_normalized_sorted_byte_array
    // expects counts to already satisfy divisibility by normalization_factor().
    for (Vertex u = 0; u < nv; ++u) {
        std::vector<std::pair<Treelet, std::uint64_t>> out_pairs;
        out_pairs.reserve(nws_counts.size());

        for (auto& kv : nws_counts) {
            const Treelet&              t   = kv.first;
            const std::vector<CountT>&  vu  = kv.second;   // per-vertex signed counts
            const CountT                sc  = vu[u];       // signed accumulation (should be >= 0 here)
            assert(sc >= 0 && "NWS per-vertex count must be non-negative before serialization");

            if (sc != 0) {
                out_pairs.emplace_back(t, static_cast<std::uint64_t>(sc));
            }
        }

        auto [buf, bytes] = builder.to_normalized_sorted_byte_array(u, out_pairs);
        output->write(buf, static_cast<std::streamsize>(bytes));
        delete[] buf;
    }

    output->flush();
}