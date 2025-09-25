#pragma once

#include <utility>
#include <unordered_set>
#include "../graph/UndirectedGraph.h"  // per vertex_t

struct PairHash {
  std::size_t operator()(const std::pair<int,int>& p) const noexcept {
    std::size_t h1 = std::hash<int>{}(p.first);
    std::size_t h2 = std::hash<int>{}(p.second);
    return h1 ^ (h2 + 0x9e3779b9 + (h1<<6) + (h1>>2));
  }
};

struct PairEqual {
  bool operator()(const std::pair<int,int>& a,
                  const std::pair<int,int>& b) const noexcept {
    return a.first == b.first && a.second == b.second;
  }
};

using Pair  = std::pair<UndirectedGraph::vertex_t, UndirectedGraph::vertex_t>;
using PairSet = std::unordered_set<Pair, PairHash, PairEqual>;

inline std::pair<UndirectedGraph::vertex_t, UndirectedGraph::vertex_t>
canon_pair(UndirectedGraph::vertex_t a, UndirectedGraph::vertex_t b) noexcept {
    if (a > b) std::swap(a, b);
    return {a, b};
}