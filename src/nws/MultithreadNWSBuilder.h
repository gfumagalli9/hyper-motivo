// MIT License
//
// MultithreadNWSBuilder — parallel Inclusion–Exclusion (NWS) pass on a hypergraph.
//
// High-level:
//   - Seeds the queue with all singleton subtypes (one hyperedge).
//   - A pool of worker threads repeatedly:
//       * takes (treelet, subtype) from a shared queue,
//       * calls NWSBuilder::build(subtype, treelet, counts) to:
//           (a) update per-vertex signed counts for that treelet,
//           (b) enumerate distinct one-edge extensions,
//       * deduplicates extensions via a global "visited" set (per treelet),
//         enqueues only new subtypes.
//   - When all tasks are consumed, the per-thread partial counts are reduced
//     (summed) into a final map and serialized.
//
// Concurrency:
//   - Work queue protected by work_mutex + work_condvar.
//   - Global visited{treelet -> set<subtype>} protected by visited_mutex.
//   - tasks_in_flight counts outstanding queue items (atomic).
//   - shutdown flag to stop workers after the queue is drained.
//
// Invariants for NWSBuilder::build():
//   - EdgeSubtype::edges and EdgeSubtype::verts are sorted ascending.

#ifndef MOTIVO_MULTITHREAD_NWS_BUILDER_H
#define MOTIVO_MULTITHREAD_NWS_BUILDER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <queue>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletList.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/types/EdgeSubtypeHash.h"
#include "NWSBuilder.h"

class MultithreadNWSBuilder {
    static_assert(std::is_signed<CountT>::value,
                  "CountT must be a signed integral type");

    struct thread_state {
        // Per-treelet accumulators: vector<CountT> sized to |V(H)|
        std::unordered_map<Treelet, std::vector<CountT>, Treelet::TreeletHash> nws_counts;
    };

public:
    MultithreadNWSBuilder(const Hypergraph*   H,
                          const TreeletList*  treelet_list,
                          const TreeletTable* treelet_table,
                          std::ostream*       output,
                          unsigned int        nthreads);

    // Runs the NWS pass and writes per-vertex records to `output`.
    void build();

private:
    // Worker main loop
    void worker_loop(unsigned thread_id, thread_state* state);

private:
    // Inputs / context
    const Hypergraph*   H;
    const TreeletList*  treelet_list;
    const TreeletTable* treelet_table;
    std::ostream*       output;
    const unsigned int  nthreads;

    // Performs single NWS step and enumerations
    NWSBuilder          builder;

    // Shared work queue of (treelet, subtype)
    std::queue<std::pair<Treelet, EdgeSubtype>> work_queue;
    std::mutex                                   work_mutex;
    std::condition_variable                      work_condvar;
    bool                                         shutdown = false;

    // Global "visited" map to avoid re-processing the same subtype per treelet
    std::unordered_map<Treelet,
                       std::unordered_set<EdgeSubtype>,
                       Treelet::TreeletHash> visited;
    std::mutex visited_mutex;

    // Count of outstanding tasks in the system (queue items)
    std::atomic<size_t> tasks_in_flight{0};
};

#endif // MOTIVO_MULTITHREAD_NWS_BUILDER_H