#include "PairIO.h"
#include <fstream>
#include <stdexcept>

PairSet load_pairs_set(const std::string& filename) {
    return PairSet::load_from_pairs(filename);
}

void save_pairs_set(const std::string& filename, const PairSet& pairs) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("PairIO: cannot open for write " + filename);

    const std::uint64_t M = pairs.size();
    out.write(reinterpret_cast<const char*>(&M), sizeof(M));

    // Ricostruisci le coppie canoniche u<v dalla CSR
    for (std::uint32_t u = 0; u < pairs.n(); ++u) {
        pairs.for_each_v(u, [&](std::uint32_t v){
            const auto uu = static_cast<UndirectedGraph::vertex_t>(u);
            const auto vv = static_cast<UndirectedGraph::vertex_t>(v);
            out.write(reinterpret_cast<const char*>(&uu), sizeof(uu));
            out.write(reinterpret_cast<const char*>(&vv), sizeof(vv));
        });
    }
}