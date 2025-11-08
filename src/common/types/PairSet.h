// motivo/src/common/types/PairSet.h
#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <cstdio>                 // FILE*, fread, fopen, rewind, fclose
#include "../graph/UndirectedGraph.h"  // per vertex_t

using Pair = std::pair<UndirectedGraph::vertex_t, UndirectedGraph::vertex_t>;

inline Pair canon_pair(UndirectedGraph::vertex_t a, UndirectedGraph::vertex_t b) noexcept {
    if (a > b) std::swap(a, b);
    return {a, b};
}

// CSR: per ogni u, v>u in [offsets_[u], offsets_[u+1]) ordinati crescenti
class PairSet {
public:
    PairSet() = default;

    // true se (u,v) è presente (accetta u>v: normalizza internamente)
    inline std::size_t count(const Pair& p) const noexcept {
        auto u = p.first, v = p.second;
        if (u == v || n_ == 0) return 0;
        if (u > v) std::swap(u, v);
        return contains_uv_normalized(u, v) ? 1u : 0u;
    }

    inline bool empty() const noexcept { return M_ == 0; }
    inline std::uint64_t size() const noexcept { return M_; } // #coppie canoniche
    inline std::uint32_t n() const noexcept { return n_; }    // #vertici

    // Accessori veloci per riga:
    //  - degree(u): #v tali che (u,v) ∈ S (con v>u)
    //  - row(u): (puntatore, len) alla riga u (array ordinato di v>u)
    inline std::uint32_t degree(std::uint32_t u) const noexcept {
        if (u >= n_) return 0u;
        return static_cast<std::uint32_t>(offsets_[u+1] - offsets_[u]);
    }
    inline std::pair<const std::uint32_t*, std::uint32_t>
    row(std::uint32_t u) const noexcept {
        if (u >= n_) return {nullptr, 0u};
        const std::uint64_t b = offsets_[u];
        const std::uint64_t e = offsets_[u+1];
        return { adj_.data() + b, static_cast<std::uint32_t>(e - b) };
    }

    // Itera i soli vicini v>u (utile per ottimizzazioni nei builder)
    template<class F>
    inline void for_each_v(UndirectedGraph::vertex_t u, F f) const {
        if (static_cast<std::uint32_t>(u) >= n_) return;
        const auto b = offsets_[u], e = offsets_[u+1];
        for (std::uint32_t i = b; i < e; ++i) f(adj_[i]);
    }

    // Costruzione da file .pairs: [uint64_t M][(u,v)*M] con coppie già canoniche (u<v)
    // In caso di errore di lettura restituisce un PairSet vuoto.
    static PairSet load_from_pairs(const std::string& filename) {
        PairSet S;

        std::FILE* fp = std::fopen(filename.c_str(), "rb");
        if (!fp) return S; // file assente = insieme vuoto

        std::uint64_t M = 0;
        if (std::fread(&M, sizeof(M), 1, fp) != 1) { std::fclose(fp); return S; }

        // Passo 1: leggi a blocchi, computa deg[u] e max vertex id
        const std::size_t BUF_PAIRS = 1u << 20; // ~1M coppie per batch
        std::vector<std::uint32_t> buf(2 * BUF_PAIRS);

        std::uint32_t maxv = 0;
        std::vector<std::uint32_t> deg; deg.reserve(1024);

        std::uint64_t read_pairs = 0;
        while (read_pairs < M) {
            const std::uint64_t todo = std::min<std::uint64_t>(BUF_PAIRS, M - read_pairs);
            const std::size_t want = static_cast<std::size_t>(2 * todo);
            const std::size_t got  = std::fread(buf.data(), sizeof(std::uint32_t), want, fp);
            if (got != want) { std::fclose(fp); return PairSet{}; }

            for (std::size_t i = 0; i < want; i += 2) {
                const auto u = buf[i], v = buf[i+1];
                if (u > v) { std::fclose(fp); return PairSet{}; } // ci aspettiamo canonico
                maxv = std::max(maxv, std::max(u, v));
            }

            if (deg.size() < static_cast<std::size_t>(maxv) + 1) deg.resize(static_cast<std::size_t>(maxv) + 1, 0);
            for (std::size_t i = 0; i < want; i += 2) {
                const auto u = buf[i];
                ++deg[u]; // CSR con soli v>u
            }
            read_pairs += todo;
        }

        S.n_ = deg.empty() ? 0u : static_cast<std::uint32_t>(deg.size());
        S.M_ = M;

        S.offsets_.assign(static_cast<std::size_t>(S.n_) + 1, 0);
        for (std::uint32_t u = 0; u < S.n_; ++u) S.offsets_[u+1] = S.offsets_[u] + deg[u];
        S.adj_.assign(static_cast<std::size_t>(S.offsets_.back()), 0);

        // cursori di scrittura per ciascun u
        std::vector<std::uint32_t> cur = S.offsets_;

        // Passo 2: riempi adj[u] con i v (in genere già in ordine se il file è sortato)
        std::rewind(fp);
        (void)std::fread(&M, sizeof(M), 1, fp); // rileggi header
        read_pairs = 0;
        while (read_pairs < M) {
            const std::uint64_t todo = std::min<std::uint64_t>(BUF_PAIRS, M - read_pairs);
            const std::size_t want = static_cast<std::size_t>(2 * todo);
            const std::size_t got  = std::fread(buf.data(), sizeof(std::uint32_t), want, fp);
            if (got != want) { std::fclose(fp); return PairSet{}; }

            for (std::size_t i = 0; i < want; i += 2) {
                const auto u = buf[i], v = buf[i+1];
                S.adj_[cur[u]++] = v;
            }
            read_pairs += todo;
        }
        std::fclose(fp);

        // Per sicurezza: garantisci ordinamento locale (se già ordinato, è no-op)
        for (std::uint32_t u = 0; u < S.n_; ++u) {
            const auto b = S.offsets_[u], e = S.offsets_[u+1];
            std::sort(S.adj_.begin() + b, S.adj_.begin() + e);
            // Facoltativo: rimuovi duplicati se il writer non li ha già tolti
            // const auto it = std::unique(S.adj_.begin()+b, S.adj_.begin()+e);
            // S.adj_.erase(it, S.adj_.begin()+e);  // richiede aggiornare offsets_ -> sconsigliato qui
        }
        return S;
    }

private:
    inline bool contains_uv_normalized(UndirectedGraph::vertex_t u,
                                       UndirectedGraph::vertex_t v) const noexcept {
        if (u >= n_ || v >= n_) return false;
        const auto b = offsets_[u], e = offsets_[u+1];
        const auto* base = adj_.data();
        return std::binary_search(base + b, base + e, static_cast<std::uint32_t>(v));
    }

    std::uint32_t n_ = 0;
    std::uint64_t M_ = 0; // #coppie canoniche
    std::vector<std::uint32_t> offsets_;  // size n_+1
    std::vector<std::uint32_t> adj_;      // concatenazione delle righe
};