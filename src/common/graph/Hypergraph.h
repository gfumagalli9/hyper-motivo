#ifndef MOTIVO_HYPERGRAPH_H
#define MOTIVO_HYPERGRAPH_H

#include <cstdint>
#include <string>
#include <vector>

class Hypergraph {
public:
    using vertex_t = uint32_t;
    using edge_t   = uint32_t;
    static constexpr vertex_t INVALID_VERTEX = UINT32_MAX;
    static constexpr edge_t   INVALID_EDGE   = UINT32_MAX;

private:
    vertex_t num_verts;
    edge_t   num_edges;

    // file descriptors
    FILE*    hef_fd;  // hyperedges → vertices offsets
    FILE*    hvd_fd;  // hyperedge vertices data
    FILE*    vhef_fd; // (opzionale) vertices → hyperedges offsets
    FILE*    vhed_fd; // (opzionale) vertex hyperedges data
    FILE*    dmap_fd; // (opzionale) mapping denso

    // mappature
    char*    offsets_he;  // (num_edges+1)*sizeof(edge_t)
    char*    verts_he;    // sum_e|HE(e)| * sizeof(vertex_t)
    char*    offsets_vh;  // (num_verts+1)*sizeof(vertex_t)
    char*    hed_vh;      // sum_v deg(v) * sizeof(edge_t)
    char*    dmap_vh;     // num_verts * sizeof(vertex_t)

    bool     has_dense_map;

    // if not empty, map 
    std::vector<vertex_t> dense2orig_;

    // calcola puntatore ai dati:
    inline char* he_offset_ptr(edge_t e, edge_t i=0) const {
        uint32_t off;
        memcpy(&off, offsets_he + e*sizeof(edge_t), sizeof(off));
        return verts_he + uint64_t(off + i)*sizeof(vertex_t);
    }
    inline char* vh_offset_ptr(vertex_t v, vertex_t i=0) const {
        uint32_t off;
        memcpy(&off, offsets_vh + v*sizeof(vertex_t), sizeof(off));
        return hed_vh + uint64_t(off + i)*sizeof(edge_t);
    }

    inline char* dmap_offset_ptr(vertex_t i) const{
        return dmap_vh + uint64_t(i) * sizeof(vertex_t);
    }

public:
    Hypergraph(const std::string& basename);
    ~Hypergraph();

    // dimensioni
    vertex_t number_of_vertices() const { return num_verts; }
    edge_t   number_of_hyperedges() const { return num_edges; }

    // iperarco e sue proprietà
    vertex_t hyperedge_size(edge_t e) const;
    vertex_t hyperedge_vertex(edge_t e, vertex_t i) const;

    // vertice e sue proprietà
    vertex_t vertex_degree(vertex_t v) const;             // num iperarcs incidenti
    // Restituisce l'i-esimo iperarco incidente a v
    edge_t   incident_hyperedge(vertex_t v, vertex_t i) const;

    // appartenenza
    bool has_incident(vertex_t v, edge_t e) const;

    vertex_t denseToOrig(vertex_t d) const;

    inline size_t max_vertex_degree() const noexcept {
        size_t maxDeg = 0;
        const auto n = number_of_vertices();
        for (vertex_t v = 0; v < n; ++v) {
            size_t d = vertex_degree(v);
            if (d > maxDeg) maxDeg = d;
        }
        return maxDeg;
    }

    // alias “grafico”
    edge_t   number_of_edges()  const { return number_of_hyperedges(); }
    vertex_t degree(vertex_t v) const { return vertex_degree(v);       }
    
    // carica preventivamente tutto in RAM
    void      prefault();
};
#endif // MOTIVO_HYPERGRAPH_H