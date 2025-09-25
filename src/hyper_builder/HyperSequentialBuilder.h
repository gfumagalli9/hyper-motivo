// MIT License
//
// HyperSequentialBuilder — sequential DP pass for hypergraphs (k >= 2).
// For each vertex u, merges:
//   - lower  C_i(u)   from GLOBAL tables (i = 1..k-1)
//   - IE     IE_j(u)  from HIGH IE tables (j = 1..k-1)
// using Treelet::merge(), and writes the size-k row for u.
//
// Notes:
//  - Uses IEBuilder internally to perform the pairwise merges and accumulation.
//  - Output format matches TreeletTable on-disk layout (per-vertex record).

#ifndef MOTIVO_HYPER_SEQUENTIAL_BUILDER_H
#define MOTIVO_HYPER_SEQUENTIAL_BUILDER_H

#include <ostream>

#include "../common/graph/Hypergraph.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "InclusionExclusionBuilder.h"                 // IEBuilder (safe_add/safe_mul via InclusionExclusionBuilder.h transitively)
#include "../builder/ColorCodingHashmap.h"        // map-like container used for accumulation

class HyperSequentialBuilder
{
private:
    const Hypergraph*                      H;            // HIGH hypergraph (for #V and vertex range)
    const Hypergraph::vertex_t             from_vertex;  // inclusive
    const Hypergraph::vertex_t             to_vertex;    // inclusive
    const unsigned int                     size;         // target k
    const TreeletTableCollection*          ttc;          // GLOBAL C_i, i = 1..k-1
    const TreeletTableCollection*          tIEc;         // HIGH   IE_j, j = 1..k-1
    const bool                             store_only_0; // if true, only store rows with color 0 root
    std::ostream*                          output;       // destination .cnt stream
    IEBuilder                              builder;      // performs the IE merging
    const bool                             normalize;    // divide by normalization_factor() on write

public:
    HyperSequentialBuilder(const Hypergraph*                 H,
                           Hypergraph::vertex_t              from_vertex,
                           Hypergraph::vertex_t              to_vertex,
                           unsigned int                      size,
                           const TreeletTableCollection*     ttc,
                           const TreeletTableCollection*     tIEc,
                           bool                              store_only_0,
                           const TreeletStructureSelector*   selector,   // optional filter (can be nullptr)
                           std::ostream*                     output,
                           bool                              normalize);

    /// Runs the sequential build on [from_vertex .. to_vertex], writing:
    ///   [num_vertices][records for u=from..to]
    /// Each record is produced by IEBuilder and appended to `output`.
    void build [[gnu::hot]] ();
};

#endif // MOTIVO_HYPER_SEQUENTIAL_BUILDER_H