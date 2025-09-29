#pragma once
// MIT License
// HyperOccurrence — canonical bipartite incidence for hypergraph motifs.
//
// What is a HyperOccurrence?
// --------------------------
// Given a sampled k-set of vertices U = (u0,...,u_{k-1}) (k<=16) from a hypergraph H,
// a HyperOccurrence encodes the *weakly-induced* sub-hypergraph on U as a bipartite
// incidence matrix M of size (V x E):
//   - rows  : the k vertices in the sampled order (one row per ui),
//   - columns: one column for each DISTINCT hyperedge intersection e∩U with |e∩U|>=2
//             (duplicates across different e that induce the same subset are collapsed).
//
// We then *canonicalize* this bipartite graph using nauty with two color classes:
//   - class A = rows (vertices in U)
//   - class B = columns (distinct e∩U blocks)
// Canonicalization is restricted within color classes (we never swap rows with columns).
// The canonical (row/column) order is used to pack M as a compact byte string that
// becomes the "fingerprint" of the hypergraphlet; this enables grouping and counting.
//
// Additionally we store compact “Gaifman bits” (k<=16) that encode the simple graph
// over U where an edge (i,j) is present iff ∃ hyperedge e with {ui,uj}⊆e. This is used
// elsewhere for rooted spanning-tree counts.
//
// Two construction modes:
//   (1) From Hypergraph H and vertex list U       → build weakly-induced incidence M
//   (2) From an explicit incidence block M        → useful for “treelet-only” mode
// Both support optional nauty-based canonicalization.
//
// Binary packing format of the incidence:
//   [ a(uint16_t), b(uint16_t), row-major bits of size a*b ]
// where a = #rows (=k), b = #columns (distinct intersections).
// This header prevents ambiguity between shapes with same bit-length.

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <climits>
#include <algorithm>

#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../sampler/include_nauty.h"

// Gaifman bits reuse the same layout as Occurrence (<=16 vertices, 15 bytes)
struct GaifmanBits {
    static constexpr unsigned int binary_footprint_bits  = 119;
    static constexpr unsigned int binary_footprint_bytes =
        (binary_footprint_bits + CHAR_BIT - 1) / CHAR_BIT;
    uint8_t bytes[binary_footprint_bytes] = {0};
};

class HyperOccurrenceCanonicizer;

class HyperOccurrence {
    friend class HyperOccurrenceCanonicizer;
public:
    using vertex_t = UndirectedGraph::vertex_t;

    HyperOccurrence() = default;

    // Build from H and U (weakly-induced mode):
    // - Build incidence on U: one column per DISTINCT e∩U with |e∩U|>=2
    // - Optionally canonicalize (V vs E color classes) using nauty
    // - Pack to a compact byte blob (with [a,b] header)
    HyperOccurrence(unsigned int k,
                    const Hypergraph* H,
                    const vertex_t* U,
                    const GaifmanBits& gaifman_bits,
                    bool canonicalize_bipartite = true);

    // Build from explicit incidence matrix (treelet-only mode, or single-pass incidence):
    // - 'incidence' is a dense 0/1 matrix with k rows and b columns.
    // - Optionally canonicalize (V vs E color classes) using nauty.
    HyperOccurrence(unsigned int k,
        const vertex_t* U,
        const GaifmanBits& gaifman_bits,
        const std::vector<std::vector<uint8_t>>& incidence,
        bool canonicalize_bipartite = true);
                    

    // Helper: incidence for a pure treelet (k rows, k-1 columns; each column is {child,parent})
    static void build_treelet_incidence_block(const Treelet& t,
                                              unsigned int k,
                                              std::vector<std::vector<uint8_t>>& M);

    // Accessors
    inline unsigned int        k()        const { return k_vertices; }
    inline const vertex_t*     vertices() const { return verts; }
    inline const GaifmanBits&  gaifman()  const { return gaifman_fp; }

    // Canonical bipartite fingerprint (packed bytes + [a,b] header)
    inline const uint8_t* bipartite_binary() const { return bipartite_bytes.data(); }
    inline size_t         bipartite_binary_size() const { return bipartite_bytes.size(); }

    // Hex view (computed lazily, useful for debugging/grouping)
    const char* bipartite_text() const;

    // Validity
    inline bool is_valid() const { return valid; }

    // Comparators for grouping/sorting by bipartite fingerprint
    struct FootprintHash {
        size_t operator()(const HyperOccurrence&  h) const noexcept;
        size_t operator()(const HyperOccurrence* h) const noexcept;
    };
    struct FootprintEq {
        bool operator()(const HyperOccurrence&  a, const HyperOccurrence&  b) const noexcept;
        bool operator()(const HyperOccurrence* a, const HyperOccurrence* b) const noexcept;
    };
    struct FootprintLess {
        bool operator()(const HyperOccurrence&  a, const HyperOccurrence&  b) const noexcept;
        bool operator()(const HyperOccurrence* a, const HyperOccurrence* b) const noexcept;
    };

private:
    // Input size (k<=16 enforced for Gaifman bits upstream)
    unsigned int k_vertices = 0;
    vertex_t     verts[16]  = {0};

    // Canonical bipartite block packed as:
    // [a(uint16_t), b(uint16_t), ceil(a*b/8) bytes], row-major (V-major)
    std::vector<uint8_t> bipartite_bytes;

    // Pretty text cache (hex)
    mutable std::string bipartite_text_cache;

    // Gaifman bits (k<=16)
    GaifmanBits gaifman_fp{};

    bool valid = false;

    // Pack VxE incidence block to bytes with shape header
    static void pack_incidence(const std::vector<std::vector<uint8_t>>& M,
                               std::vector<uint8_t>* out);

    // Build weakly-induced incidence restricted to U (simple hypergraph):
    // returns VxE block M (a=k rows, b columns = unique e∩U, |e∩U|>=2)
    static void build_incidence_block(const Hypergraph* H,
                                      const vertex_t* U,
                                      unsigned int k,
                                      std::vector<std::vector<uint8_t>>& M);
};

// Thread-local nauty workspace: builds canonical labeling for a bipartite incidence
// with two color classes (rows then columns), and permutes M accordingly.
class HyperOccurrenceCanonicizer {
public:
    HyperOccurrenceCanonicizer();
    ~HyperOccurrenceCanonicizer();

    // Canonicalize a bipartite incidence matrix M (size a x b, binary).
    // The final order respects color classes: [rows(A)] then [cols(B)].
    void canonicalize_bipartite(std::vector<std::vector<uint8_t>>& M);

private:
    // dynamic buffers (reallocated when N changes)
    nauty_graph* g      = nullptr;
    nauty_graph* cang   = nullptr;
    int*         lab    = nullptr;
    int*         ptn    = nullptr;
    int*         orbits = nullptr;

    size_t words_needed = 0;
    int    N_alloc      = 0;

    DEFAULTOPTIONS_GRAPH(options);
    statsblk stats{};

    void ensure_capacity(int N);
    void reset_dyn();
};