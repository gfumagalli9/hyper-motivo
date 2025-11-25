// MIT License
//
// SequentialNWSBuilder — sequential Inclusion–Exclusion (NWS) pass on a hypergraph.
//
// For a given list of treelets, performs the NWS accumulation:
//   - starts from all singleton subtypes (one hyperedge);
//   - expands subtypes breadth-first via NWSBuilder::build(), which both
//     (a) updates per-vertex NWS counts for the current subtype,
//     (b) enumerates all distinct “one-more-edge” extensions;
//   - avoids reprocessing the same subtype per treelet with a per-treelet
//     visited set;
//   - finally, for each vertex u, serializes the (treelet,count) pairs
//     using the same on-disk layout as TreeletTable.
//
// Invariants required by NWSBuilder::build():
//   - EdgeSubtype::edges and EdgeSubtype::verts are sorted in ascending order.

#ifndef MOTIVO_SEQUENTIAL_NWS_BUILDER_H
#define MOTIVO_SEQUENTIAL_NWS_BUILDER_H

#include <cstdint>
#include <ostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../common/graph/Hypergraph.h"
#include "../common/treelets/TreeletList.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/Treelet.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/types/EdgeSubtypeHash.h"
#include "NWSBuilder.h"

class SequentialNWSBuilder {
public:
    using Vertex = Hypergraph::vertex_t;

    // CountT is defined in NWSBuilder.h and must be a signed integral type.
    static_assert(std::is_signed<CountT>::value,
                  "CountT must be a signed integral type");

    /// Construct the sequential NWS builder.
    /// - H:            input hypergraph
    /// - treelet_list: list of treelets to process
    /// - treelet_table: (read-only) per-vertex counts used by NWSBuilder to
    ///                  retrieve C(T, v) (the builder does not mutate it)
    /// - output:       destination stream for the serialized per-vertex rows
    SequentialNWSBuilder(const Hypergraph*   H,
                         const TreeletList*  treelet_list,
                         const TreeletTable* treelet_table,
                         std::ostream*       output,
                         bool                allow_singletons) noexcept;

    /// Run the NWS pass and write the per-vertex records.
    void build();

private:
    const Hypergraph*   H;
    const TreeletList*  treelet_list;
    const TreeletTable* treelet_table;
    std::ostream*       output;
    bool                allow_singletons;

    // Accumulators: for each treelet → vector of per-vertex signed counts
    std::unordered_map<Treelet, std::vector<CountT>, Treelet::TreeletHash> nws_counts;

    // BFS queue over (treelet, subtype)
    std::queue<std::pair<Treelet, EdgeSubtype>> work_queue;

    // Per-treelet visited subtypes (to avoid duplicate work)
    std::unordered_map<Treelet,
                       std::unordered_set<EdgeSubtype>,
                       Treelet::TreeletHash> visited;

    // Performs one NWS step + enumerates extensions
    NWSBuilder builder;
};

#endif // MOTIVO_SEQUENTIAL_NWS_BUILDER_H