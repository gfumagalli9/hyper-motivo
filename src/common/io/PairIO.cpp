#include "PairIO.h"
#include <fstream>
#include <stdexcept>
#include "../graph/Hypergraph.h"

PairSet load_pairs_set(const std::string& filename, bool canonicalize) {
    PairSet S;
    load_pairs_append(filename, S, canonicalize, /*clear_out=*/false);
    return S;
}

void load_pairs_append(const std::string& filename, PairSet& out,
                       bool canonicalize, bool clear_out) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        // File assente: lascia 'out' com’è (utile per workflow opzionali)
        return;
    }
    if (clear_out) out.clear();

    std::uint64_t M = 0;
    in.read(reinterpret_cast<char*>(&M), sizeof(M));
    if (!in) throw std::runtime_error("PairIO: cannot read pair count from " + filename);

    // Un po' di slack per ridurre rehash
    out.reserve(out.size() + static_cast<size_t>(M * 2));

    for (std::uint64_t i = 0; i < M; ++i) {
        Hypergraph::vertex_t u, v;
        in.read(reinterpret_cast<char*>(&u), sizeof(u));
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (!in) throw std::runtime_error("PairIO: truncated file " + filename);

        if (canonicalize) out.insert(canon_pair(u, v));
        else             out.insert({u, v});
    }
}

void save_pairs_set(const std::string& filename, const PairSet& pairs) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("PairIO: cannot open for write " + filename);

    const std::uint64_t M = static_cast<std::uint64_t>(pairs.size());
    out.write(reinterpret_cast<const char*>(&M), sizeof(M));

    for (const auto& p : pairs) {
        const auto& u = p.first;
        const auto& v = p.second;
        out.write(reinterpret_cast<const char*>(&u), sizeof(u));
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
}