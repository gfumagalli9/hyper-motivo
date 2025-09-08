// EdgeSubtypeHash.h
#pragma once
#include "EdgeSubtype.h"
#include <functional>

// 2) specializzazione di std::hash
namespace std {
  template<>
  struct hash<EdgeSubtype> {
    size_t operator()(EdgeSubtype const& st) const noexcept {
      size_t h = 0;
      // combinatore tipo boost::hash_combine
      auto mix = [](size_t& seed, size_t v){
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2);
      };

      // incorporate sum
      mix(h, std::hash<int64_t>{}(st.sum));

      // incorporate ogni edge
      for (auto e : st.edges)
        mix(h, std::hash<Hypergraph::edge_t>{}(e));

      // incorporate ogni vertice
      for (auto v : st.verts)
        mix(h, std::hash<Hypergraph::vertex_t>{}(v));

      return h;
    }
  };
}