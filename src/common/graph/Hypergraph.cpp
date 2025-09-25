#include "Hypergraph.h"

#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>   // fileno
#include <cstring>    // std::memcpy

#include "../platform/platform.h"

// Companion files layout (see header for overview).
// This implementation mmaps each region read-only. On any failure, it throws.

Hypergraph::Hypergraph(const std::string& basename) {
    // --- Read metadata (.hmeta) ----------------------------------------------
    const std::string meta_fn = basename + ".hmeta";
    if (FILE* meta = std::fopen(meta_fn.c_str(), "rb")) {
        if (std::fread(&num_verts, sizeof(vertex_t), 1, meta) != 1 ||
            std::fread(&num_edges, sizeof(edge_t),   1, meta) != 1) {
            std::fclose(meta);
            throw std::runtime_error("Hypergraph: failed to read metadata: " + meta_fn);
        }
        std::fclose(meta);
    } else {
        throw std::runtime_error("Hypergraph: cannot open metadata: " + meta_fn);
    }

    // --- Open required HE->V files (.hef / .hvd) ------------------------------
    const std::string hef_fn = basename + ".hef";
    const std::string hvd_fn = basename + ".hvd";

    hef_fd = std::fopen(hef_fn.c_str(), "rb");
    if (!hef_fd) throw std::runtime_error("Hypergraph: cannot open " + hef_fn);

    hvd_fd = std::fopen(hvd_fn.c_str(), "rb");
    if (!hvd_fd) {
        std::fclose(hef_fd);
        throw std::runtime_error("Hypergraph: cannot open " + hvd_fn);
    }

    // --- Open optional V->HE files (.vhef / .vhed) ----------------------------
    const std::string vhef_fn = basename + ".vhef";
    const std::string vhed_fn = basename + ".vhed";
    vhef_fd = std::fopen(vhef_fn.c_str(), "rb");
    vhed_fd = std::fopen(vhed_fn.c_str(), "rb");
    const bool have_vmap = (vhef_fd && vhed_fd);

    // --- Open optional dense map (.dmap) --------------------------------------
    const std::string dmap_fn = basename + ".dmap";
    dmap_fd       = std::fopen(dmap_fn.c_str(), "rb");
    has_dense_map = (dmap_fd != nullptr);

    // --- Mmap HE->V offsets (.hef) -------------------------------------------
    {
        const size_t bytes = static_cast<size_t>(num_edges + 1) * sizeof(uint32_t);
        offsets_he = static_cast<char*>(motivo_mmap(bytes, PROT_READ, fileno(hef_fd)));
        if (offsets_he == MAP_FAILED) throw std::runtime_error("Hypergraph: mmap failed " + hef_fn);
    }

    // --- Mmap HE->V payload (.hvd) -------------------------------------------
    {
        struct stat st{};
        if (fstat(fileno(hvd_fd), &st) != 0)
            throw std::runtime_error("Hypergraph: stat failed " + hvd_fn);
        if (st.st_size % sizeof(vertex_t) != 0)
            throw std::runtime_error("Hypergraph: corrupted payload size " + hvd_fn);

        verts_he = static_cast<char*>(motivo_mmap(static_cast<size_t>(st.st_size),
                                                 PROT_READ, fileno(hvd_fd)));
        if (verts_he == MAP_FAILED) throw std::runtime_error("Hypergraph: mmap failed " + hvd_fn);
    }

    // --- Mmap optional V->HE mapping (.vhef / .vhed) --------------------------
    if (have_vmap) {
        const size_t vhef_bytes = static_cast<size_t>(num_verts + 1) * sizeof(uint32_t);
        offsets_vh = static_cast<char*>(motivo_mmap(vhef_bytes, PROT_READ, fileno(vhef_fd)));
        if (offsets_vh == MAP_FAILED) throw std::runtime_error("Hypergraph: mmap failed " + vhef_fn);

        struct stat st{};
        if (fstat(fileno(vhed_fd), &st) != 0)
            throw std::runtime_error("Hypergraph: stat failed " + vhed_fn);
        if (st.st_size % sizeof(edge_t) != 0)
            throw std::runtime_error("Hypergraph: corrupted payload size " + vhed_fn);

        hed_vh = static_cast<char*>(motivo_mmap(static_cast<size_t>(st.st_size),
                                               PROT_READ, fileno(vhed_fd)));
        if (hed_vh == MAP_FAILED) throw std::runtime_error("Hypergraph: mmap failed " + vhed_fn);
    }

    // --- Mmap optional dense map (.dmap) --------------------------------------
    if (has_dense_map) {
        const size_t bytes = static_cast<size_t>(num_verts) * sizeof(vertex_t);
        dmap_vh = static_cast<char*>(motivo_mmap(bytes, PROT_READ, fileno(dmap_fd)));
        if (dmap_vh == MAP_FAILED) throw std::runtime_error("Hypergraph: mmap failed " + dmap_fn);
    }
}

Hypergraph::~Hypergraph() {
    // Unmap + close hef
    if (offsets_he) {
        const size_t bytes = static_cast<size_t>(num_edges + 1) * sizeof(uint32_t);
        motivo_munmap(offsets_he, bytes);
        offsets_he = nullptr;
    }
    if (hef_fd) { std::fclose(hef_fd); hef_fd = nullptr; }

    // Unmap + close hvd
    if (verts_he) {
        struct stat st{};
        if (hvd_fd && fstat(fileno(hvd_fd), &st) == 0)
            motivo_munmap(verts_he, static_cast<size_t>(st.st_size));
        verts_he = nullptr;
    }
    if (hvd_fd) { std::fclose(hvd_fd); hvd_fd = nullptr; }

    // Unmap + close vhef/vhed (optional)
    if (offsets_vh) {
        const size_t bytes = static_cast<size_t>(num_verts + 1) * sizeof(uint32_t);
        motivo_munmap(offsets_vh, bytes);
        offsets_vh = nullptr;
    }
    if (vhef_fd) { std::fclose(vhef_fd); vhef_fd = nullptr; }

    if (hed_vh) {
        struct stat st{};
        if (vhed_fd && fstat(fileno(vhed_fd), &st) == 0)
            motivo_munmap(hed_vh, static_cast<size_t>(st.st_size));
        hed_vh = nullptr;
    }
    if (vhed_fd) { std::fclose(vhed_fd); vhed_fd = nullptr; }

    // Unmap + close dmap (optional)
    if (dmap_vh) {
        const size_t bytes = static_cast<size_t>(num_verts) * sizeof(vertex_t);
        motivo_munmap(dmap_vh, bytes);
        dmap_vh = nullptr;
    }
    if (dmap_fd) { std::fclose(dmap_fd); dmap_fd = nullptr; }
}

// --- Hyperedge access ---------------------------------------------------------

Hypergraph::vertex_t
Hypergraph::hyperedge_size(edge_t e) const {
    // size = offsets_he[e+1] - offsets_he[e]
    const char* begin = he_offset_ptr(e);
    const char* end   = he_offset_ptr(e + 1);
    return static_cast<vertex_t>((end - begin) / sizeof(vertex_t));
}

Hypergraph::vertex_t
Hypergraph::hyperedge_vertex(edge_t e, vertex_t i) const {
    vertex_t v{};
    std::memcpy(&v, he_offset_ptr(e, i), sizeof(vertex_t));
    return v;
}

// --- Vertex access (require V->HE) -------------------------------------------

Hypergraph::vertex_t
Hypergraph::vertex_degree(vertex_t v) const {
    if (!offsets_vh) {
        throw std::runtime_error("Hypergraph: vertex_degree() requires .vhef/.vhed");
    }
    if (v >= num_verts) {
        throw std::out_of_range("Hypergraph::vertex_degree: vertex out of range");
    }
    const char* begin = vh_offset_ptr(v);
    const char* end   = vh_offset_ptr(v + 1);
    return static_cast<vertex_t>((end - begin) / sizeof(edge_t));
}

Hypergraph::edge_t
Hypergraph::incident_hyperedge(vertex_t v, vertex_t i) const {
    if (!offsets_vh) {
        throw std::runtime_error("Hypergraph: incident_hyperedge() requires .vhef/.vhed");
    }
    edge_t e{};
    std::memcpy(&e, vh_offset_ptr(v, i), sizeof(edge_t));
    return e;
}

// --- Membership check ---------------------------------------------------------

bool
Hypergraph::has_incident(vertex_t v, edge_t e) const {
    if (v >= num_verts || e >= num_edges) return false;

    // Prefer V->HE if available (edge ids are sorted there).
    if (offsets_vh) {
        const char* begin = vh_offset_ptr(v);
        const char* end   = vh_offset_ptr(v + 1);

        while (begin < end) {
            const char* mid = begin + ((end - begin) / (2 * sizeof(edge_t))) * sizeof(edge_t);
            edge_t t{};
            std::memcpy(&t, mid, sizeof(edge_t));
            if (t == e) return true;
            if (e < t)  end   = mid;
            else        begin = mid + sizeof(edge_t);
        }
        return false;
    }

    // Fallback: binary search inside HE->V list (vertex ids sorted there).
    const char* begin = he_offset_ptr(e);
    const char* end   = he_offset_ptr(e + 1);

    while (begin < end) {
        const char* mid = begin + ((end - begin) / (2 * sizeof(vertex_t))) * sizeof(vertex_t);
        vertex_t u{};
        std::memcpy(&u, mid, sizeof(vertex_t));
        if (u == v) return true;
        if (v < u)  end   = mid;
        else        begin = mid + sizeof(vertex_t);
    }
    return false;
}

// --- Dense map ---------------------------------------------------------------

Hypergraph::vertex_t
Hypergraph::denseToOrig(vertex_t d) const {
    if (!has_dense_map) return d;
    if (d >= num_verts) {
        throw std::out_of_range("Hypergraph::denseToOrig: vertex out of range");
    }
    vertex_t v{};
    std::memcpy(&v, dmap_offset_ptr(d), sizeof(vertex_t));
    return v;
}

// --- Prefault ----------------------------------------------------------------

void Hypergraph::prefault() {
    // .hef
    {
        const size_t bytes = static_cast<size_t>(num_edges + 1) * sizeof(uint32_t);
        motivo_prefault(0, bytes, fileno(hef_fd));
    }
    // .hvd
    {
        struct stat st{};
        if (fstat(fileno(hvd_fd), &st) == 0) {
            motivo_prefault(0, static_cast<size_t>(st.st_size), fileno(hvd_fd));
        }
    }
    // .vhef / .vhed (optional)
    if (offsets_vh) {
        const size_t vhef_bytes = static_cast<size_t>(num_verts + 1) * sizeof(uint32_t);
        motivo_prefault(0, vhef_bytes, fileno(vhef_fd));

        struct stat st{};
        if (fstat(fileno(vhed_fd), &st) == 0) {
            motivo_prefault(0, static_cast<size_t>(st.st_size), fileno(vhed_fd));
        }
    }
    // .dmap (optional)
    if (dmap_vh) {
        const size_t bytes = static_cast<size_t>(num_verts) * sizeof(vertex_t);
        motivo_prefault(0, bytes, fileno(dmap_fd));
    }
}

// --- Shared-hyperedge utilities (require V->HE) -------------------------------

std::size_t
Hypergraph::count_shared_hyperedges(vertex_t u, vertex_t v) const {
    std::size_t cnt = 0;
    for_each_shared_hyperedge(u, v, [&](edge_t){ ++cnt; });
    return cnt;
}

bool
Hypergraph::share_any_hyperedge(vertex_t u, vertex_t v) const {
    bool found = false;
    for_each_shared_hyperedge(u, v, [&](edge_t){ found = true; });
    return found;
}