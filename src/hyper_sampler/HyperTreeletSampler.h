// MIT License
// HyperTreeletSampler — sampling of rooted occurrences on a split hypergraph.
// Uses global DP tables (root/global) for kT/kS/kP and NWS on HIGH only.

#pragma once
#include <cstdint>
#include <vector>
#include <cassert>

#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"

// The sampler assumes LOW/HIGH were built with a shared coloring and that
// the "root" global tables (ttc_root) contain k = 1..K counts.

class HyperTreeletSampler {
public:
    using vertex_t = UndirectedGraph::vertex_t;

    /// Minimal constructor (preferred).
    /// @param G_low     Gaifman graph for the LOW part
    /// @param H_large   HIGH hypergraph
    /// @param ttc_root  global DP tables (kT/kS/kP) used for masses and complements
    /// @param nws_high  NWS tables on HIGH (used only for the HIGH part)
    /// @param size      motif size k (used by sample_root/sample_treelet)
    HyperTreeletSampler(const UndirectedGraph *G_low,
                        const Hypergraph *H_large,
                        const TreeletTableCollection *ttc_root,
                        const TreeletTableCollection *nws_high,
                        unsigned int size);

    // --- Minimal API (mirrors TreeletSampler) -----------------

    /// Sample a root according to the global table for size k.
    inline vertex_t sample_root(Random *rng) const {
        return ttc_root_->get_table(k_)->get_random_root(rng);
    }

    /// Sample a colored treelet T of size k rooted at 'root'.
    inline Treelet sample_treelet(vertex_t root, Random *rng) const {
        Treelet t = ttc_root_->get_table(k_)->get_random_treelet(root, rng);
        assert(t.is_valid());
        return t;
    }

    /// Sample one rooted occurrence of treelet t at root u into occ[0..kT-1].
    /// Returns false only if the tables give zero mass (defensive).
    bool sample_rooted_occurrence(const Treelet &t,
                                  vertex_t u,
                                  vertex_t *occ,
                                  Random *rng);

private:
    // Tables used by a single split of T into parent (kP) and child (kS).
    struct SamplerTables {
        TreeletTable* TtabG = nullptr; // kT, global table for T
        TreeletTable* CtabG = nullptr; // kP, global table for parent(T \ child)
        TreeletTable* StabG = nullptr; // kS, global table for split child (LOW view)
        TreeletTable* NWSh  = nullptr; // kS, NWS over HIGH (rooted at u)
    };

    struct DecompChoice {
        Treelet  parent  = invalid_treelet;     // T \ child
        Treelet  child   = invalid_treelet;     // split child
        vertex_t child_v = static_cast<vertex_t>(-1); // chosen neighbor/root for child
    };

    bool     get_tables_(uint32_t kT, uint32_t kS, uint32_t kP, SamplerTables& out) const;
    uint64_t compute_low_mass_(vertex_t u, const Treelet& t, const Treelet& split, const SamplerTables& tb) const;
    uint64_t compute_high_mass_(vertex_t u, const Treelet& t, const Treelet& split, const SamplerTables& tb) const;
    bool     choose_low_(vertex_t u, const Treelet& t, const Treelet& split, uint64_t W_low,  const SamplerTables& tb, DecompChoice& out, Random* rng) const;
    bool     choose_high_(vertex_t u, const Treelet& t, const Treelet& split, uint64_t W_high, const SamplerTables& tb, DecompChoice& out, Random* rng) const;
    bool     overlap_reject_half_(vertex_t u, vertex_t v, Random* rng) const;

    /// Count hyperedges incident to both v and u (used for HIGH acceptance correction).
    uint32_t common_incident_count(vertex_t v, vertex_t u) const;

private:
    const UndirectedGraph        *G_low_    = nullptr;
    const Hypergraph             *H_large_  = nullptr;
    const TreeletTableCollection *ttc_root_ = nullptr;
    const TreeletTableCollection *nws_high_ = nullptr;
    const unsigned int            k_        = 0;
};