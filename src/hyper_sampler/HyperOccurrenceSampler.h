#pragma once
#include <vector>
#include <thread>
#include <limits>
#include <cassert>
#include <cstring> // memcpy

#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../sampler/DynamicSequencer.h"
#include "../sampler/TimeoutThreadSync.h"

#include "HyperTreeletSampler.h"
#include "HyperOccurrence.h"
#include "HyperSampleTable.h"

// -----------------------------------------------------------------------------
// HyperOccurrenceSampler
// -----------------------------------------------------------------------------
// Samples k-vertex hypergraph motifs using the LOW/HIGH engine (HyperTreeletSampler).
// For each sample, it produces:
//   (1) Gaifman bits of the weakly-induced subhypergraph on the sampled vertices
//   (2) A canonical bipartite incidence (V/E color classes) as the motif ID
//
// If `graphlets == true`: we build the weakly-induced subhypergraph (edges are
// the restrictions e∩U with |e∩U|>=2), and canonicalize the VxE incidence.
//
// If `graphlets == false`: we build a degenerate incidence with (k-1) columns,
// one per treelet edge (child,parent), mirroring Occurrence(Treelet, occ).
//
// Canonicalization of the bipartite block can be toggled via `canonicize`.
// -----------------------------------------------------------------------------
class HyperOccurrenceSampler {
private:
    using sequencer_t = DynamicSequencer<uint64_t>;

    // Source hypergraph used to build the weakly-induced incidence/Gaifman
    const Hypergraph*    H;
    const unsigned int   size;        // k (<= 16)
    const bool           graphlets;   // true: weakly-induced hypergraphlet; false: treelet-only incidence
    const bool           canonicize;  // canonicalize V/E bipartite incidence

    // LOW/HIGH sampling engine
    HyperTreeletSampler  sampler;

    // Worker: pull batches from the sequencer, produce HyperOccurrence objects
    void sample_thread [[gnu::hot, gnu::flatten]] (unsigned int thread_no,
                                                   std::vector<HyperOccurrence>& samples,
                                                   sequencer_t* sequencer,
                                                   Random* rng,
                                                   TimeoutThreadSync& sync);

    // Compute Gaifman bits of the weakly-induced subhypergraph on U:
    // for each hyperedge e incident to any u∈U, add all pairs in (e∩U).
    void build_weak_induced(const UndirectedGraph::vertex_t* U,
                            unsigned k,
                            uint8_t* out_bits);

    // Build Gaifman bits and the VxE incidence in a **single pass** over
    // incident hyperedges. This avoids scanning the hypergraph twice when
    // graphlets=true. The incidence is returned as a dense 0/1 matrix M (k rows).
    void build_weak_and_incidence(const UndirectedGraph::vertex_t* U,
        unsigned k,
        uint8_t* out_bits,
        std::vector<std::vector<uint8_t>>& M);

    // Same layout as Occurrence::add_edge (upper-triangular, row-major)
    static inline void set_edge_bit(uint8_t* bits, unsigned i, unsigned j) {
        if (i == j) return;
        if (i < j) std::swap(i, j);
        const unsigned pos = (i - 1) * i / 2 + j;
        bits[pos / 8] |= static_cast<uint8_t>(0x80u >> (pos % 8));
    };

public:
    // Produce exactly one HyperOccurrence:
    //  - draws a root and a treelet
    //  - samples a rooted occurrence (k vertices) with the LOW/HIGH engine
    //  - builds Gaifman bits
    //  - builds (and optionally canonicalizes) the VxE incidence block
    inline void sample_one [[gnu::hot]] (HyperOccurrence* occurrence, Random* rng) {
        assert(size <= 16 && "HyperOccurrenceSampler supports k <= 16");

        UndirectedGraph::vertex_t U[16] = {0};
        const UndirectedGraph::vertex_t root = sampler.sample_root(rng);
        const Treelet t = sampler.sample_treelet(root, rng);

        // Retry until the LOW/HIGH recursion returns a valid k-tuple
        while (true) {
            const bool ok = sampler.sample_rooted_occurrence(t, root, U, rng);
            if (!ok) continue;

            // 1) Build Gaifman bits (15 bytes) on U
            uint8_t bits[GaifmanBits::binary_footprint_bytes] = {0};
            GaifmanBits gbits;

            if (graphlets) {
                // NEW: single-pass over hyperedges to build both Gaifman and incidence.
                std::vector<std::vector<uint8_t>> M; // k x b (0/1)
                build_weak_and_incidence(U, size, bits, M);
                std::memcpy(gbits.bytes, bits, GaifmanBits::binary_footprint_bytes);

                // Build HyperOccurrence from the pre-built incidence (no second scan).
                new (occurrence) HyperOccurrence(size, U, gbits, M,
                                                 /*canonicalize_bipartite=*/canonicize);
            } else {
                // Treelet-only edges (child,parent) as in Occurrence(Treelet, occ)
                unsigned int parents[16] = {0};
                unsigned int current = 0, n = 0;
                for (Treelet::treelet_structure_t s = t.get_structure(); s; s <<= 1) {
                    if (s & Treelet::treelet_structure_highest_bit) {
                        ++n;
                        set_edge_bit(bits, n, current);
                        parents[n] = current;
                        current = n;
                    } else {
                        current = parents[current];
                    }
                }
                assert(n == size - 1);

                std::memcpy(gbits.bytes, bits, GaifmanBits::binary_footprint_bytes);

                // Degenerate incidence: one column per treelet edge
                std::vector<std::vector<uint8_t>> M;
                HyperOccurrence::build_treelet_incidence_block(t, size, M);
                new (occurrence) HyperOccurrence(size, U, gbits, M,
                                                 /*canonicalize_bipartite=*/canonicize);
            }
            break;
        }
    }

    // Parallel sampling API: returns a HyperSampleTable with n_samples
    // (or runs until time_budget if n_samples == 0).
    HyperSampleTable* sample(uint64_t n_samples,
                             unsigned int number_of_threads,
                             Random* rng,
                             double time_budget = std::numeric_limits<double>::infinity());

    // Constructor: keeps H, k, flags; forwards all tables/graphs to the sampler.
    // Unused parameters are kept for API parity and explicitly ignored.
    HyperOccurrenceSampler(const UndirectedGraph* gaifman_graph,
                           const Hypergraph*      H_large,
                           const Hypergraph*      H,
                           const TreeletTableCollection* ttc_root,
                           const TreeletTableCollection* nws_high,
                           unsigned int size,
                           bool /*vertices*/,   // unused
                           bool graphlets,
                           bool canonicize)
    : H(H),
      size(size),
      graphlets(graphlets),
      canonicize(canonicize),
      sampler(gaifman_graph, H_large, ttc_root, nws_high, size) {}
};