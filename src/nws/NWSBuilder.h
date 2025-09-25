// MIT License
//
// NWSBuilder — single NWS step on hypergraphs
// -------------------------------------------
// Given a subtype st (a set of hyperedges and their common vertex set st.verts)
// and a treelet T, this class:
//   1) Updates the per-vertex NWS counters using an inclusion-exclusion style
//      contribution derived from the counts C_T(v) for v in st.verts.
//   2) Enumerates all unique one-edge extensions st' = st ∪ {e}, where e is an
//      hyperedge incident to any u ∈ st.verts, and st'.verts = st.verts ∩ e.
//      Only extensions with |st'.verts| > 1 are kept.
//
// Notes:
//   - The table used to read C_T(v) is the "NWS table" (TreeletTable) passed
//     in the constructor (read-only).
//   - The template parameter T for `build` is any associative container
//     mapping Treelet -> count (e.g., ColorCodingHashmap). The template
//     parameter for `nws_counts` is any random-access container indexed by
//     vertex id (e.g., std::vector<CountT>).
//   - We assume st.edges and st.verts are sorted ascending; this is required
//     by std::set_intersection and for binary_search/lower_bound.
//   - 128-bit accumulators are used to safely sum counts before narrowing.

#ifndef MOTIVO_NWS_BUILDER_H
#define MOTIVO_NWS_BUILDER_H

#include <algorithm>  // set_intersection, sort, binary_search, lower_bound
#include <cassert>
#include <cstdint>
#include <cstring>    // memcpy
#include <limits>
#include <new>        // placement new
#include <vector>

#include "../common/graph/Hypergraph.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/Treelet.h"

using Vertex     = Hypergraph::vertex_t;
using Edge       = Hypergraph::edge_t;
using CountT     = int64_t;
using VertexList = std::vector<Vertex>;
using EdgeSubset = std::vector<Edge>;

class NWSBuilder {
private:
    const Hypergraph*   H;          // read-only HIGH hypergraph
    const TreeletTable* nws_table;  // read-only per-vertex counts C_T(v)

public:
    explicit NWSBuilder(const Hypergraph* H, const TreeletTable* nws_table) noexcept
        : H(H), nws_table(nws_table) {}

    // --------------------------------------------------------------------
    // build(st, T, nws_counts) -> vector<EdgeSubtype>
    // --------------------------------------------------------------------
    // 1) Inclusion-exclusion update:
    //    Let S = st.verts and C_T(v) be the count for treelet T at vertex v.
    //    Define SUM = Σ_{x∈S} C_T(x).
    //    For every x ∈ S, we add:
    //        Δ(x) = (-1)^{|st|} * ( SUM - C_T(x) )
    //    to nws_counts[x]. (Project convention: sign is derived from |st|.)
    //
    // 2) One-edge extensions:
    //    For each u ∈ S and each hyperedge e incident to u:
    //      - if e ∈ st.edges, skip;
    //      - otherwise, st' = st ∪ {e} with st'.edges kept sorted;
    //      - st'.verts = S ∩ verts(e), keep only if |st'.verts| > 1;
    //      - compute SUM' = Σ_{v∈st'.verts} C_T(v) and store it in subtype.
    //
    // Returns all distinct extensions st' (deduplicated by edge set).
    //
    // T: associative container Treelet -> CountT   (accumulator)
    // nws_counts: random-access container indexed by Vertex (e.g. vector<CountT>)
    template<typename VertexCounts>
    inline std::vector<EdgeSubtype>
    build [[gnu::hot]] (const EdgeSubtype& st, const Treelet& T, VertexCounts& nws_counts) const
    {
        // Preconditions (debug): st.verts and st.edges must be sorted asc
        assert(std::is_sorted(st.verts.begin(), st.verts.end()));
        assert(std::is_sorted(st.edges.begin(), st.edges.end()));

        const std::uint16_t level = static_cast<std::uint16_t>(st.edges.size());
        // Project convention for the sign on this layer:
        //   level odd  -> +1
        //   level even -> -1
        const CountT sign = (level & 1) ? +1 : -1;

        std::vector<EdgeSubtype> st_next;
        st_next.reserve(8); // small heuristic; typical fan-out per layer is modest

        // ---- Inclusion-exclusion style update on current st ----
        __int128 sum128 = 0;
        for (Vertex x : st.verts) {
            sum128 += static_cast<__int128>(nws_table->get_count(x, T));
        }
        // Narrow with range check before reusing the value
        assert(sum128 >= static_cast<__int128>(std::numeric_limits<CountT>::min()) &&
               sum128 <= static_cast<__int128>(std::numeric_limits<CountT>::max()));
        const CountT SUM = static_cast<CountT>(sum128);

        for (Vertex x : st.verts) {
            __int128 delta128 = static_cast<__int128>(SUM) -
                                static_cast<__int128>(nws_table->get_count(x, T));
            if (sign < 0) delta128 = -delta128;   // apply (-1)^{|st|}
            assert(delta128 >= static_cast<__int128>(std::numeric_limits<CountT>::min()) &&
                   delta128 <= static_cast<__int128>(std::numeric_limits<CountT>::max()));
            nws_counts[x] += static_cast<CountT>(delta128);
        }

        // ---- Generate all unique one-edge extensions of st ----
        for (Vertex u : st.verts) {
            const std::uint32_t deg = H->vertex_degree(u);
            for (std::uint32_t j = 0; j < deg; ++j) {
                const Edge e = H->incident_hyperedge(u, j);

                // Skip edges already present in st
                if (std::binary_search(st.edges.begin(), st.edges.end(), e)) continue;

                // Build sorted new edge-set new_E = st.edges ∪ {e}
                EdgeSubset new_edges = st.edges;
                new_edges.insert(std::lower_bound(new_edges.begin(), new_edges.end(), e), e);

                // Deduplicate: if we already generated an extension with the
                // same edge-set, skip. (O(#extensions) scan; fine for small fan-out)
                bool seen = false;
                for (const auto& s : st_next) {
                    if (s.edges == new_edges) { seen = true; break; }
                }
                if (seen) continue;

                // Intersect current vertex set with verts(e) to get new verts
                VertexList e_verts;
                const std::uint32_t esz = H->hyperedge_size(e);
                e_verts.reserve(esz);
                for (std::uint32_t i = 0; i < esz; ++i) {
                    e_verts.push_back(H->hyperedge_vertex(e, i));
                }
                // We rely on both inputs being sorted
                assert(std::is_sorted(e_verts.begin(), e_verts.end()));

                VertexList inters;
                inters.reserve(std::min(st.verts.size(), e_verts.size()));
                std::set_intersection(st.verts.begin(), st.verts.end(),
                                      e_verts.begin(), e_verts.end(),
                                      std::back_inserter(inters));

                // Keep only "non-degenerate" intersections (>=2 vertices)
                if (inters.size() <= 1) continue;

                // Pre-compute SUM' = Σ_{v∈inters} C_T(v) and store it in the subtype
                __int128 is128 = 0;
                for (Vertex v : inters) {
                    is128 += static_cast<__int128>(nws_table->get_count(v, T));
                }
                assert(is128 >= static_cast<__int128>(std::numeric_limits<CountT>::min()) &&
                       is128 <= static_cast<__int128>(std::numeric_limits<CountT>::max()));
                const CountT sum_inters = static_cast<CountT>(is128);

                st_next.push_back(EdgeSubtype{std::move(new_edges), std::move(inters), sum_inters});
            }
        }

        return st_next;
    }

    // --------------------------------------------------------------------
    // to_normalized_sorted_byte_array(u, table)
    // --------------------------------------------------------------------
    // Pack the accumulator "table" for vertex u into a contiguous byte buffer
    // exactly as TreeletTable expects on disk:
    //   [vertex_t u][uint64_t n][n * treelet_count_pair]
    // Pairs are sorted by Treelet to guarantee deterministic layout.
    //
    // The caller owns the returned buffer and must delete[] result.first.
    template<typename AccTable>
    inline std::pair<char*, std::size_t>
    to_normalized_sorted_byte_array [[gnu::hot]] (const Hypergraph::vertex_t u,
                                                  const AccTable& table) const
    {
        using Pair = TreeletTable::treelet_count_pair;

        std::pair<char*, std::size_t> out;
        const std::size_t n = table.size();

        out.second = sizeof(Hypergraph::vertex_t) + sizeof(std::uint64_t) + n * sizeof(Pair);
        out.first  = new char[out.second];

        // Header
        std::memcpy(out.first, &u, sizeof(Hypergraph::vertex_t));
        const std::uint64_t n64 = static_cast<std::uint64_t>(n);
        std::memcpy(out.first + sizeof(Hypergraph::vertex_t), &n64, sizeof(std::uint64_t));

        // Placement-new the array of pairs in the payload area
        static_assert((sizeof(Hypergraph::vertex_t) + sizeof(std::uint64_t)) %
                          alignof(Pair) == 0,
                      "treelet_count_pair not aligned in buffer");

        auto* pairs = new (out.first + sizeof(Hypergraph::vertex_t) + sizeof(std::uint64_t)) Pair[n];

        // Fill pairs (no normalization here; caller ensures counts already normalized if needed)
        std::size_t i = 0;
        for (auto it = table.begin(); it != table.end(); ++it, ++i) {
            // Project invariants
            assert(it->second > 0);
            assert(it->second % it->first.normalization_factor() == 0);

            pairs[i].treelet = it->first;
            pairs[i].count   = it->second;
        }

        // Stable, deterministic order
        std::sort(pairs, pairs + n,
                  [](const Pair& a, const Pair& b) { return a.treelet < b.treelet; });

        return out;
    }
};

#endif // MOTIVO_NWS_BUILDER_H