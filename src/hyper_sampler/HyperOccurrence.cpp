#include "HyperOccurrence.h"
#include <unordered_map>
#include <unordered_set>
#include <cstring>

// ---------------------- Packing helpers -----------------------

void HyperOccurrence::pack_incidence(const std::vector<std::vector<uint8_t>>& M,
                                     std::vector<uint8_t>* out)
{
    out->clear();
    const uint16_t a = static_cast<uint16_t>(M.size());
    const uint16_t b = static_cast<uint16_t>(M.empty() ? 0 : M[0].size());

    // Shape header [a,b] (big-endian for stability)
    out->push_back(static_cast<uint8_t>(a >> 8));
    out->push_back(static_cast<uint8_t>(a & 0xFF));
    out->push_back(static_cast<uint8_t>(b >> 8));
    out->push_back(static_cast<uint8_t>(b & 0xFF));

    // Row-major bits
    const size_t bits  = static_cast<size_t>(a) * static_cast<size_t>(b);
    const size_t bytes = (bits + 7) / 8;
    out->resize(4 + bytes, 0);

    size_t bitpos = 0;
    for (uint16_t i = 0; i < a; ++i) {
        assert(M[i].size() == b);
        for (uint16_t j = 0; j < b; ++j, ++bitpos) {
            if (M[i][j]) {
                const size_t byte = 4 + (bitpos >> 3);
                const size_t off  = bitpos & 7;
                (*out)[byte] |= static_cast<uint8_t>(0x80u >> off);
            }
        }
    }
}

// ---------------------- Incidence builder ---------------------

// Weakly-induced incidence on U:
// - Iterate all hyperedges touching U (dedup by edge-id).
// - For each edge e, compute mask m over U (bit i set iff ui∈e).
// - Keep m iff popcount(m) >= 2 (weakly-induced).
// - Deduplicate masks across edges (set of uint32_t).
// - Build matrix M (rows=U, cols=distinct masks). Columns are sorted for determinism.
void HyperOccurrence::build_incidence_block(const Hypergraph* H,
                                            const vertex_t* U,
                                            unsigned int k,
                                            std::vector<std::vector<uint8_t>>& M)
{
    assert(H != nullptr);
    assert(k >= 1 && k <= 16);

    // Map vertex -> row index in U
    std::unordered_map<vertex_t,int> idx;
    idx.reserve(k * 2);
    for (unsigned i = 0; i < k; ++i)
        idx.emplace(U[i], static_cast<int>(i));

    // Collect distinct masks and avoid revisiting the same hyperedge
    std::unordered_set<uint32_t> masks_set;
    masks_set.reserve(64);
    std::vector<uint32_t> masks; masks.reserve(32);

    std::unordered_set<uint32_t> seen_edges; // dedup edge-ids
    seen_edges.reserve(128);

    for (unsigned i = 0; i < k; ++i) {
        const auto v  = U[i];
        const uint32_t dv = H->vertex_degree(v);
        for (uint32_t t = 0; t < dv; ++t) {
            const uint32_t e = H->incident_hyperedge(v, t);
            if (!seen_edges.insert(e).second) continue; // process each e once

            uint32_t m = 0;
            const uint32_t sz = H->hyperedge_size(e);
            for (uint32_t j = 0; j < sz; ++j) {
                const auto w = H->hyperedge_vertex(e, j);
                const auto it = idx.find(w);
                if (it != idx.end()) m |= (1u << it->second);
            }

            if ((m & (m - 1)) != 0) {  // at least 2 vertices from U in the hyperedge
                if (masks_set.insert(m).second) masks.push_back(m);
            }
        }
    }

    const uint16_t a = static_cast<uint16_t>(k);
    const uint16_t b = static_cast<uint16_t>(masks.size());
    M.assign(a, std::vector<uint8_t>(b, 0));

    // Sort columns for determinism before (optional) canonicalization
    std::sort(masks.begin(), masks.end());

    for (uint16_t j = 0; j < b; ++j) {
        const uint32_t m = masks[j];
        for (uint16_t i = 0; i < a; ++i)
            if (m & (1u << i)) M[i][j] = 1;
    }
}

// ---------------------- Treelet incidence ---------------------

void HyperOccurrence::build_treelet_incidence_block(const Treelet& treelet,
                                                    unsigned int k,
                                                    std::vector<std::vector<uint8_t>>& M)
{
    // Reproduce (child,parent) edges as in Occurrence(Treelet, occ)
    unsigned int parents[16] = {0};
    unsigned int current = 0, n = 0;

    std::vector<std::pair<unsigned,int>> edges;
    edges.reserve(k > 0 ? (k - 1) : 0);

    for (auto structure = treelet.get_structure(); structure; structure <<= 1) {
        if (structure & Treelet::treelet_structure_highest_bit) {
            ++n;
            edges.emplace_back(n, static_cast<int>(current)); // (child=n, parent=current)
            parents[n] = current;
            current = n;
        } else {
            current = parents[current];
        }
    }
    assert(n == k - 1);

    const uint16_t a = static_cast<uint16_t>(k);
    const uint16_t b = static_cast<uint16_t>(edges.size());
    M.assign(a, std::vector<uint8_t>(b, 0));

    for (uint16_t j = 0; j < b; ++j) {
        const unsigned ch = edges[j].first;
        const unsigned pr = static_cast<unsigned>(edges[j].second);
        M[ch][j] = 1;
        M[pr][j] = 1;
    }
}

// ---------------------- Constructors --------------------------

HyperOccurrence::HyperOccurrence(unsigned int k,
                                 const vertex_t* U,
                                 const GaifmanBits& gbits,
                                 const std::vector<std::vector<uint8_t>>& incidence,
                                 bool canonicalize_bipartite)
: k_vertices(k), gaifman_fp(gbits)
{
    assert(k >= 1 && k <= 16);
    for (unsigned i = 0; i < k; ++i) verts[i] = U[i];

    std::vector<std::vector<uint8_t>> M = incidence; // local copy
    if (canonicalize_bipartite) {
        static thread_local HyperOccurrenceCanonicizer canon;
        canon.canonicalize_bipartite(M);
    }

    pack_incidence(M, &bipartite_bytes);
    valid = true;
}

HyperOccurrence::HyperOccurrence(unsigned int k,
                                 const Hypergraph* H,
                                 const vertex_t* U,
                                 const GaifmanBits& gbits,
                                 bool canonicalize_bipartite)
: k_vertices(k), gaifman_fp(gbits)
{
    assert(k >= 1 && k <= 16);
    for (unsigned i = 0; i < k; ++i) verts[i] = U[i];

    // Build VxE incidence (restricted to U, |e∩U|>=2)
    std::vector<std::vector<uint8_t>> M;
    build_incidence_block(H, U, k, M);

    // Canonicalize within color classes (V vs E) if requested
    if (canonicalize_bipartite) {
        static thread_local HyperOccurrenceCanonicizer canon;
        canon.canonicalize_bipartite(M);
    }

    pack_incidence(M, &bipartite_bytes);
    valid = true;
}

// ---------------------- Fingerprint string --------------------

const char* HyperOccurrence::bipartite_text() const
{
    if (!bipartite_text_cache.empty()) return bipartite_text_cache.c_str();

    static const char hex[] = "0123456789ABCDEF";
    bipartite_text_cache.reserve(2 * bipartite_bytes.size());
    for (uint8_t c : bipartite_bytes) {
        bipartite_text_cache.push_back(hex[c >> 4]);
        bipartite_text_cache.push_back(hex[c & 0x0F]);
    }
    return bipartite_text_cache.c_str();
}

// ---------------------- Hash/Compare --------------------------

static inline size_t fnv1a_64(const uint8_t* p, size_t n) {
    size_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

size_t HyperOccurrence::FootprintHash::operator()(const HyperOccurrence& h) const noexcept {
    return fnv1a_64(h.bipartite_binary(), h.bipartite_binary_size());
}
size_t HyperOccurrence::FootprintHash::operator()(const HyperOccurrence* h) const noexcept {
    return fnv1a_64(h->bipartite_binary(), h->bipartite_binary_size());
}
bool HyperOccurrence::FootprintEq::operator()(const HyperOccurrence& a, const HyperOccurrence& b) const noexcept {
    const size_t na = a.bipartite_binary_size(), nb = b.bipartite_binary_size();
    return (na == nb) && (std::memcmp(a.bipartite_binary(), b.bipartite_binary(), na) == 0);
}
bool HyperOccurrence::FootprintEq::operator()(const HyperOccurrence* a, const HyperOccurrence* b) const noexcept {
    const size_t na = a->bipartite_binary_size(), nb = b->bipartite_binary_size();
    return (na == nb) && (std::memcmp(a->bipartite_binary(), b->bipartite_binary(), na) == 0);
}
bool HyperOccurrence::FootprintLess::operator()(const HyperOccurrence& a, const HyperOccurrence& b) const noexcept {
    const size_t na = a.bipartite_binary_size(), nb = b.bipartite_binary_size();
    if (na != nb) return na < nb;
    return std::memcmp(a.bipartite_binary(), b.bipartite_binary(), na) < 0;
}
bool HyperOccurrence::FootprintLess::operator()(const HyperOccurrence* a, const HyperOccurrence* b) const noexcept {
    const size_t na = a->bipartite_binary_size(), nb = b->bipartite_binary_size();
    if (na != nb) return na < nb;
    return std::memcmp(a->bipartite_binary(), b->bipartite_binary(), na) < 0;
}

// ---------------------- Nauty canonicizer ---------------------

HyperOccurrenceCanonicizer::HyperOccurrenceCanonicizer() {
    options.getcanon   = MOTIVO_NAUTY_TRUE;
    options.defaultptn = 0; // we provide color classes via ptn[]
    options.digraph    = 0; // undirected bipartite
}

HyperOccurrenceCanonicizer::~HyperOccurrenceCanonicizer() {
    reset_dyn();
    nauty_freedyn();
    nautil_freedyn();
    naugraph_freedyn();
}

void HyperOccurrenceCanonicizer::reset_dyn() {
    delete[] g;      g = nullptr;
    delete[] cang;   cang = nullptr;
    delete[] lab;    lab = nullptr;
    delete[] ptn;    ptn = nullptr;
    delete[] orbits; orbits = nullptr;
    words_needed = 0; N_alloc = 0;
}

void HyperOccurrenceCanonicizer::ensure_capacity(int N) {
#ifndef NDEBUG
    if (N <= 0) { reset_dyn(); return; }
#endif
    const size_t wn = static_cast<size_t>(SETWORDSNEEDED(N));
    if (N == N_alloc && wn == words_needed && g) return;

    reset_dyn();
    words_needed = wn;
    N_alloc      = N;

    g      = new nauty_graph[static_cast<size_t>(N) * wn];
    cang   = new nauty_graph[static_cast<size_t>(N) * wn];
    lab    = new int[N];
    ptn    = new int[N];
    orbits = new int[N];
}

void HyperOccurrenceCanonicizer::canonicalize_bipartite(std::vector<std::vector<uint8_t>>& M) {
#ifndef NDEBUG
    nauty_check(MOTIVO_NAUTY_WORDSIZE, SETWORDSNEEDED(1), 1, NAUTYVERSIONID);
#endif
    const int a = static_cast<int>(M.size());
    const int b = (a == 0) ? 0 : static_cast<int>(M[0].size());
    const int N = a + b;
    ensure_capacity(N);

    // lab: initial labeling 0..N-1
    // ptn: partition markers (1 within a cell, 0 at cell end)
    for (int i = 0; i < N; ++i) { lab[i] = i; ptn[i] = 1; }
    if (a > 0) ptn[a - 1] = 0; // end of A-class (rows)
    if (b > 0) ptn[N - 1] = 0; // end of B-class (cols)

    EMPTYGRAPH(g, words_needed, N);

    // Add edges between row i and column (a+j) when M[i][j] = 1
    for (int i = 0; i < a; ++i)
        for (int j = 0; j < b; ++j)
            if (M[i][j])
                ADDONEEDGE(g, i, a + j, static_cast<int>(words_needed));

    // Canonical labeling (lab[]), canonical graph into cang (unused here)
    densenauty(g, lab, ptn, orbits, &options, &stats,
               static_cast<int>(words_needed), N, cang);

    // Extract canonical order, preserving color classes:
    // nauty provides a total order over 0..N-1; split it into A (rows) then B (cols)
    std::vector<int> Aorder; Aorder.reserve(a);
    std::vector<int> Border; Border.reserve(b);
    for (int t = 0; t < N; ++t) {
        const int v = lab[t];
        if (v < a)      Aorder.push_back(v);
        else            Border.push_back(v - a);
    }

    // Build M' = M[Aorder, Border]
    std::vector<std::vector<uint8_t>> Mc(a, std::vector<uint8_t>(b, 0));
    for (int i = 0; i < a; ++i)
        for (int j = 0; j < b; ++j)
            Mc[i][j] = M[Aorder[i]][Border[j]];

    M.swap(Mc);
}