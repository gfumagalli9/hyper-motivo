// MIT License
//
// HyperMultithreadedBuilder — parallel DP pass for hypergraphs (k >= 2).
// Each thread repeatedly grabs the next vertex u in [from..to], runs
// IEBuilder::combine(u, table) against GLOBAL (C_i) and HIGH-IE (IE_j)
// tables, then writes the per-vertex record via ConcurrentWriter.
//
// Notes:
//  - Parallelism is per-vertex (coarse grain), as IEBuilder::combine(u, ...)
//    already encapsulates all pairwise merges needed for u.
//  - Output layout matches TreeletTable's on-disk format (per-vertex record).
//
// Dependencies:
//  - ../common/graph/Hypergraph.h
//  - ../common/treelets/TreeletTableCollection.h
//  - ../common/treelets/TreeletStructureSelector.h
//  - ../builder/ColorCodingHashmap.h
//  - ../common/io/ConcurrentWriter.h
//  - InclusionExclusionBuilder.h (IEBuilder)

#ifndef MOTIVO_HYPER_MULTITHREADED_BUILDER_H
#define MOTIVO_HYPER_MULTITHREADED_BUILDER_H

#include <atomic>
#include <ostream>
#include <thread>
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "../builder/ColorCodingHashmap.h"
#include "../common/io/ConcurrentWriter.h"
#include "InclusionExclusionBuilder.h" // IEBuilder

class HyperMultithreadedBuilder
{
private:
    using V = Hypergraph::vertex_t;

    static_assert(std::atomic<bool>::is_always_lock_free,
                  "std::atomic<bool> is not always lock free");
    static_assert(std::atomic<V>::is_always_lock_free,
                  "std::atomic<vertex_t> is not always lock free");

    // Inputs / configuration
    const Hypergraph*                 H;            // HIGH hypergraph
    const V                           from_vertex;  // inclusive
    const V                           to_vertex;    // inclusive
    const unsigned int                size;         // target k (>=2)
    const TreeletTableCollection*     ttc;          // GLOBAL C_i (i=1..k-1)
    const TreeletTableCollection*     tIEc;         // HIGH   IE_j (j=1..k-1)
    const bool                        store_only_0; // store only roots with color 0
    std::ostream*                     output;       // destination .cnt stream
    IEBuilder                         builder;      // IE merging core
    const bool                        normalize;    // normalize on write
    const unsigned int                nthreads;

    // Work scheduler
    std::atomic<V>                    next_vertex {Hypergraph::INVALID_VERTEX};

    // Worker loop
    void thread_loop [[gnu::hot]] (unsigned int thread_no, ConcurrentWriter* writer);

public:
    HyperMultithreadedBuilder(const Hypergraph*                 H,
                              V                                 from_vertex,
                              V                                 to_vertex,
                              unsigned int                      size,
                              const TreeletTableCollection*     ttc,
                              const TreeletTableCollection*     tIEc,
                              bool                              store_only_0,
                              const TreeletStructureSelector*   selector,   // may be nullptr
                              std::ostream*                     output,
                              unsigned int                      nthreads,
                              bool                              normalize);

    /// Write header + parallel per-vertex records.
    void build();
};

#endif // MOTIVO_HYPER_MULTITHREADED_BUILDER_H