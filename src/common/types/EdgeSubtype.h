#pragma once
#include <vector>
#include "../graph/Hypergraph.h"

struct EdgeSubtype {
    std::vector<Hypergraph::edge_t>   edges;
    std::vector<Hypergraph::vertex_t> verts;
    int64_t                           sum;

    // 1) operatore di uguaglianza
    bool operator==(EdgeSubtype const& o) const noexcept {
        return sum    == o.sum
            && edges  == o.edges
            && verts  == o.verts;
    }
};