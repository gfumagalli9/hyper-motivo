#include "Hypergraph.h"
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include "../platform/platform.h"

Hypergraph::Hypergraph(const std::string& basename) {
    // 1) Leggi metadati
    std::string meta_fn = basename + ".hmeta";
    FILE* meta_fd = fopen(meta_fn.c_str(), "rb");
    if (!meta_fd) {
        throw std::runtime_error("Unable to open metadata file: " + meta_fn);
    }
    // num_verts e num_edges
    if (fread(&num_verts, sizeof(vertex_t), 1, meta_fd) != 1 ||
        fread(&num_edges, sizeof(edge_t), 1, meta_fd) != 1) {
        fclose(meta_fd);
        throw std::runtime_error("Failed to read metadata from: " + meta_fn);
    }
    fclose(meta_fd);

    // 2) Apri file di iperarco -> vertici
    std::string hef_fn = basename + ".hef";
    hef_fd = fopen(hef_fn.c_str(), "rb");
    if (!hef_fd) {
        throw std::runtime_error("Unable to open hyperedge offsets file: " + hef_fn);
    }
    std::string hvd_fn = basename + ".hvd";
    hvd_fd = fopen(hvd_fn.c_str(), "rb");
    if (!hvd_fd) {
        fclose(hef_fd);
        throw std::runtime_error("Unable to open hyperedge data file: " + hvd_fn);
    }

    // 3) Apri file di vertice -> ipararco (opzionale)
    std::string vhef_fn = basename + ".vhef";
    vhef_fd = fopen(vhef_fn.c_str(), "rb");
    std::string vhed_fn = basename + ".vhed";
    vhed_fd = fopen(vhed_fn.c_str(), "rb");
    // Se uno dei due è assente, consideriamo la mappatura v->he disabilitata
    bool have_vmap = (vhef_fd && vhed_fd);

    // Apri file mapping se presente
    std::string dmap_fn = basename + ".dmap";
    dmap_fd = fopen(dmap_fn.c_str(), "rb");
    bool have_dmap = dmap_fd;

    // 4) Calcola dimensioni dei dati e mappa in memoria
    // 4.1 offsets_he: (num_edges+1) elementi edge_t
    size_t hef_size = (size_t)(num_edges + 1) * sizeof(edge_t);
    offsets_he = static_cast<char*>(
        motivo_mmap(hef_size, PROT_READ, fileno(hef_fd))
    );
    if (offsets_he == MAP_FAILED) {
        throw std::runtime_error("mmap failed for " + hef_fn);
    }

    // 4.2 verts_he: tutti i vertex_t nel file hvd
    struct stat st;
    if (fstat(fileno(hvd_fd), &st) < 0) {
        throw std::runtime_error("stat failed for " + hvd_fn);
    }
    size_t total_verts = st.st_size / sizeof(vertex_t);
    verts_he = static_cast<char*>(
        motivo_mmap(total_verts * sizeof(vertex_t), PROT_READ, fileno(hvd_fd))
    );
    if (verts_he == MAP_FAILED) {
        throw std::runtime_error("mmap failed for " + hvd_fn);
    }

    // 4.3 Se presente, mappa anche v->he
    if (have_vmap) {
        // offsets_vh: (num_verts+1) elementi vertex_t
        size_t vhef_size = (size_t)(num_verts + 1) * sizeof(vertex_t);
        offsets_vh = static_cast<char*>(
            motivo_mmap(vhef_size, PROT_READ, fileno(vhef_fd))
        );
        if (offsets_vh == MAP_FAILED) {
            throw std::runtime_error("mmap failed for " + vhef_fn);
        }
        // hed_vh: tutti gli edge_t nel file vhed
        if (fstat(fileno(vhed_fd), &st) < 0) {
            throw std::runtime_error("stat failed for " + vhed_fn);
        }
        size_t total_inc = st.st_size / sizeof(edge_t);
        hed_vh = static_cast<char*>(
            motivo_mmap(total_inc * sizeof(edge_t), PROT_READ, fileno(vhed_fd))
        );
        if (hed_vh == MAP_FAILED) {
            throw std::runtime_error("mmap failed for " + vhed_fn);
        }
    } else {
        offsets_vh = nullptr;
        hed_vh     = nullptr;
    }

    if(have_dmap){
        has_dense_map = true;
        dmap_vh = static_cast<char*>(motivo_mmap(total_verts * sizeof(vertex_t), PROT_READ, fileno(dmap_fd)));
        if (hed_vh == MAP_FAILED) {
            throw std::runtime_error("mmap failed for " + dmap_fn);
        }
    }else{
        dmap_vh = nullptr;
        has_dense_map = false;
    }
}

Hypergraph::~Hypergraph() {
    // Unmap e chiudi hef (offsets_he)
    if (offsets_he) {
        size_t hef_size = (size_t)(num_edges + 1) * sizeof(edge_t);
        motivo_munmap(offsets_he, hef_size);
    }
    if (hef_fd) {fclose(hef_fd);}
    // Unmap e chiudi hvd (verts_he)
    if (verts_he) {
        struct stat st;
        if (fstat(fileno(hvd_fd), &st) == 0) {
            motivo_munmap(verts_he, st.st_size);
        }
    }
    if (hvd_fd) {fclose(hvd_fd);}
    // Unmap e chiudi invert mapping v->he
    if (offsets_vh) {
        size_t vhef_size = (size_t)(num_verts + 1) * sizeof(vertex_t);
        motivo_munmap(offsets_vh, vhef_size);
    }
    if (vhef_fd) {fclose(vhef_fd);}
    if (hed_vh) {
        struct stat st;
        if (fstat(fileno(vhed_fd), &st) == 0) {motivo_munmap(hed_vh, st.st_size);}
    }
    if (vhed_fd) {fclose(vhed_fd);}
}

Hypergraph::vertex_t Hypergraph::hyperedge_size(Hypergraph::edge_t e) const {
    // Puntatori all'inizio e fine della lista di vertici di e
    char* begin = he_offset_ptr(e);
    char* end   = he_offset_ptr(e + 1);
    // Numero di elementi = byte totali diviso sizeof(vertex_t)
    return static_cast<vertex_t>((end - begin) / sizeof(vertex_t));
}

Hypergraph::vertex_t Hypergraph::hyperedge_vertex(Hypergraph::edge_t e, Hypergraph::vertex_t i) const {
    Hypergraph::vertex_t v;
    memcpy(&v, he_offset_ptr(e, i), sizeof(vertex_t));
    return v;
}

// Numero di iperarcs incidenti al vertice v
Hypergraph::vertex_t Hypergraph::vertex_degree(Hypergraph::vertex_t v) const {
    if (!offsets_vh) throw std::runtime_error("Invert mapping not available");
    // se v non esiste
    if (v >= num_verts) throw std::out_of_range("Hypergraph::vertex_degree: vertex index " + std::to_string(v) + " out of range");
    // inizio e fine lista degli iperarcs che contengono v
    char* begin = vh_offset_ptr(v);
    char* end   = vh_offset_ptr(v + 1);
    return static_cast<vertex_t>((end - begin) / sizeof(edge_t));
}

// Restituisce l'i-esimo iperarco incidente a v
Hypergraph::edge_t Hypergraph::incident_hyperedge(Hypergraph::vertex_t v, Hypergraph::vertex_t i) const {
    if (!offsets_vh) throw std::runtime_error("Invert mapping not available");
    Hypergraph::edge_t e;
    memcpy(&e, vh_offset_ptr(v, i), sizeof(edge_t));
    return e;
}

// Verifica se il vertice v è incidente all'iperarco e
bool Hypergraph::has_incident(Hypergraph::vertex_t v, Hypergraph::edge_t e) const {
    // Controlli su range
    if (v >= num_verts || e >= num_edges) return false;

    // Se abbiamo il mapping vertice->iperarco, cerco in quello (lista di edge_t)
    if (offsets_vh) {
        const char* begin = vh_offset_ptr(v);
        const char* end   = vh_offset_ptr(v + 1);
        // ricerca binaria su edge_t ordinati
        Hypergraph::edge_t t;
        while (begin < end) {
            const char* mid = begin + ((end - begin) / (2 * sizeof(edge_t))) * sizeof(edge_t);
            memcpy(&t, mid, sizeof(edge_t));
            if (t == e) return true;
            if (e < t)
                end = mid;
            else
                begin = mid + sizeof(edge_t);
        }
        return false;
    }

    // Altrimenti cerco nella lista di vertici dell'iperarco (lista di vertex_t)
    const char* begin = he_offset_ptr(e);
    const char* end   = he_offset_ptr(e + 1);
    Hypergraph::vertex_t u;
    while (begin < end) {
        const char* mid = begin + ((end - begin) / (2 * sizeof(vertex_t))) * sizeof(vertex_t);
        memcpy(&u, mid, sizeof(vertex_t));
        if (u == v) return true;
        if (v < u)
            end = mid;
        else
            begin = mid + sizeof(vertex_t);
    }
    return false;
}

Hypergraph::vertex_t Hypergraph::denseToOrig(Hypergraph::vertex_t d) const{
    if(has_dense_map){
        Hypergraph::vertex_t origin_v;
        if (d >= num_verts) {
            throw std::out_of_range(
                "Hypergraph::denseToOrig: indice d=" + std::to_string(d) +
                " fuori range [0," + std::to_string(num_verts) + ")"
            );
        }
        const char * offset = dmap_offset_ptr(d);
        memcpy(&origin_v, offset, sizeof(vertex_t));
        return origin_v;
    }else{
        return d;
    }
}

// Carica preventivamente tutte le pagine in RAM per hmap e optional vmap
void Hypergraph::prefault() {
    // Prefault degli offset e dei dati hyperedge -> vertices
    size_t hef_bytes = (size_t)(num_edges + 1) * sizeof(edge_t);
    motivo_prefault(0, hef_bytes, fileno(hef_fd));
    struct stat st;
    if (fstat(fileno(hvd_fd), &st) == 0) {
        motivo_prefault(0, st.st_size, fileno(hvd_fd));
    }

    // Se presente, prefault del mapping vertice -> hyperedges
    if (offsets_vh) {
        size_t vhef_bytes = (size_t)(num_verts + 1) * sizeof(vertex_t);
        motivo_prefault(0, vhef_bytes, fileno(vhef_fd));
        if (fstat(fileno(vhed_fd), &st) == 0) {
            motivo_prefault(0, st.st_size, fileno(vhed_fd));
        }
    }
}
    
