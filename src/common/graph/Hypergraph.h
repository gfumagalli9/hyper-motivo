#ifndef MOTIVO_HYPERGRAPH_H
#define MOTIVO_HYPERGRAPH_H

#include <cstdint>
#include <cstddef>
#include <cstdio>    // FILE*
#include <cstring>   // std::memcpy
#include <string>
#include <vector>

/**
 * Hypergraph
 * ----------
 * Read-only hypergraph stored across companion binary files:
 *
 *   <base>.hmeta : [vertex_t num_verts][edge_t num_edges]
 *   <base>.hef   : (num_edges + 1) uint32 offsets into <base>.hvd
 *   <base>.hvd   : concatenated vertex_t lists (one per hyperedge)
 *   <base>.vhef  : (num_verts + 1) uint32 offsets into <base>.vhed     [optional]
 *   <base>.vhed  : concatenated edge_t lists (incident hyperedges)      [optional]
 *   <base>.dmap  : num_verts entries of vertex_t (dense->original id)   [optional]
 *
 * Notes:
 * - HE->V (hef/hvd) is always required.
 * - V->HE (vhef/vhed) is optional, but required for degree()/incident_hyperedge()
 *   and for algorithms that iterate incident hyperedges efficiently (NWS, IE).
 * - Dense map (.dmap) is optional; if missing, denseToOrig(v) == v.
 *
 * The class mmaps the files via platform helpers (motivo_mmap/munmap/prefault).
 */
class Hypergraph {
public:
    // Basic types
    using vertex_t = uint32_t;
    using edge_t   = uint32_t;

    // Invalid sentinels
    static constexpr vertex_t INVALID_VERTEX = UINT32_MAX;
    static constexpr edge_t   INVALID_EDGE   = UINT32_MAX;

private:
    // --- Metadata -------------------------------------------------------------
    vertex_t num_verts = 0;
    edge_t   num_edges = 0;

    // --- Owned file handles (closed in ~Hypergraph) ---------------------------
    FILE* hef_fd  = nullptr; // offsets for HE->V
    FILE* hvd_fd  = nullptr; // payload (vertex lists)
    FILE* vhef_fd = nullptr; // offsets for V->HE (optional)
    FILE* vhed_fd = nullptr; // payload (edge lists) (optional)
    FILE* dmap_fd = nullptr; // dense->original vertex id (optional)

    // --- Raw mapped regions (byte pointers) ----------------------------------
    // See companion .cpp for exact element types/sizes.
    char* offsets_he = nullptr; // (num_edges + 1) uint32
    char* verts_he   = nullptr; // array of vertex_t
    char* offsets_vh = nullptr; // (num_verts + 1) uint32   [optional]
    char* hed_vh     = nullptr; // array of edge_t          [optional]
    char* dmap_vh    = nullptr; // array of vertex_t        [optional]

    bool  has_dense_map = false;

    // --- Pointer arithmetic helpers (alignment-safe via memcpy) --------------
    inline char* he_offset_ptr(edge_t e, edge_t i = 0) const {
        // offsets_he[e] gives start index in verts_he for hyperedge e
        uint32_t off = 0;
        std::memcpy(&off,
                    offsets_he + static_cast<uint64_t>(e) * sizeof(uint32_t),
                    sizeof(off));
        return verts_he + static_cast<uint64_t>(off + i) * sizeof(vertex_t);
    }

    inline char* vh_offset_ptr(vertex_t v, vertex_t i = 0) const {
        // offsets_vh[v] gives start index in hed_vh for vertex v
        uint32_t off = 0;
        std::memcpy(&off,
                    offsets_vh + static_cast<uint64_t>(v) * sizeof(uint32_t),
                    sizeof(off));
        return hed_vh + static_cast<uint64_t>(off + i) * sizeof(edge_t);
    }

    inline char* dmap_offset_ptr(vertex_t i) const {
        return dmap_vh + static_cast<uint64_t>(i) * sizeof(vertex_t);
    }

public:
    // --- Construction / Destruction ------------------------------------------
    explicit Hypergraph(const std::string& basename);
    ~Hypergraph();

    // --- Sizes ---------------------------------------------------------------
    vertex_t number_of_vertices()   const { return num_verts;  }
    edge_t   number_of_hyperedges() const { return num_edges;  }

    // --- Hyperedge access (always available: requires HE->V) -----------------
    vertex_t hyperedge_size(edge_t e) const;                          // |HE(e)|
    vertex_t hyperedge_vertex(edge_t e, vertex_t i) const;            // i-th vertex of HE(e)

    // --- Vertex access (requires optional V->HE mapping) ---------------------
    vertex_t vertex_degree(vertex_t v) const;                          // deg(v)
    edge_t   incident_hyperedge(vertex_t v, vertex_t i) const;         // i-th incident hyperedge

    // --- Membership check ----------------------------------------------------
    // Returns true iff v ∈ HE(e). Uses V->HE when available; otherwise
    // binary searches inside the HE->V list of e.
    bool has_incident(vertex_t v, edge_t e) const;

    // --- Dense-to-original id mapping ----------------------------------------
    // If .dmap is present, returns original id; otherwise, identity.
    vertex_t denseToOrig(vertex_t d) const;

    // --- Optional: fault-in mapped regions (reduces first-touch latency) -----
    void prefault();

    // --- Utilities over shared hyperedges (require V->HE) --------------------
    // For every hyperedge incident to BOTH u and v, calls fn(e).
    // Complexity: O(deg(u) + deg(v)). Requires V->HE mapping.
    template <class Fn>
    inline void for_each_shared_hyperedge(vertex_t u, vertex_t v, Fn&& fn) const {
        const uint32_t du = vertex_degree(u);
        const uint32_t dv = vertex_degree(v);
        uint32_t iu = 0, iv = 0;

        while (iu < du && iv < dv) {
            edge_t eu = incident_hyperedge(u, iu);
            edge_t ev = incident_hyperedge(v, iv);
            if (eu == ev) { fn(eu); ++iu; ++iv; }
            else if (eu < ev) { ++iu; }
            else { ++iv; }
        }
    }

    // Counts hyperedges shared by u and v. Requires V->HE mapping.
    std::size_t count_shared_hyperedges(vertex_t u, vertex_t v) const;

    // True if u and v share ≥1 hyperedge. Requires V->HE mapping.
    bool share_any_hyperedge(vertex_t u, vertex_t v) const;
};

#endif // MOTIVO_HYPERGRAPH_H