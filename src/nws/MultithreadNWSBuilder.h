// SequentialNWSBuilder.h
#ifndef MOTIVO_MULTITHREAD_NWS_BUILDER_H
#define MOTIVO_MULTITHREAD_NWS_BUILDER_H

#include <cstdint>
#include <vector>
#include <ostream>
#include <utility>    // for std::pair
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <queue>
#include <condition_variable>
#include "../common/treelets/TreeletList.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/graph/Hypergraph.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/types/EdgeSubtypeHash.h"
#include "NWSBuilder.h"

class MultithreadNWSBuilder {
private:
    struct thread_state{
        std::unordered_map<Treelet, std::vector<int>, Treelet::TreeletHash> nws_counts;  // [treelet_index][vertex]
        std::atomic<bool>   terminate{false};
        std::atomic<size_t> next_work{0};
    };
    
    const Hypergraph*      H;
    const TreeletList*     treelet_list;
    const TreeletTable*    treelet_table;
    std::ostream*          output;
    NWSBuilder             builder;

    std::queue<std::pair<Treelet, EdgeSubtype>>   work_queue;
    std::mutex                                    work_mutex;
    std::condition_variable                       work_condvar;
    bool                                          shutdown = false;

    // visited globali
    std::unordered_map<Treelet, std::unordered_set<EdgeSubtype>, Treelet::TreeletHash> visited;
    std::mutex                                                                         visited_mutex;

    std::atomic<size_t> tasks_in_flight{0};

    unsigned int nthreads;

    void worker_loop(int thread_id, thread_state* state);

public:
    MultithreadNWSBuilder(const Hypergraph* H, const TreeletList* treelet_list, const TreeletTable* treelet_table, std::ostream* output, unsigned int nthreads);

    void build();
};

#endif // MOTIVO_MULTITHREAD_NWS_BUILDER_H