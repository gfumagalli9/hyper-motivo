// MIT License

#include "MultithreadNWSBuilder.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

MultithreadNWSBuilder::MultithreadNWSBuilder(const Hypergraph*   H,
                                             const TreeletList*  treelet_list,
                                             const TreeletTable* treelet_table,
                                             std::ostream*       output,
                                             unsigned int        nthreads,
                                             bool                allow_singletons)
    : H(H)
    , treelet_list(treelet_list)
    , treelet_table(treelet_table)
    , output(output)
    , nthreads(nthreads)
    , allow_singletons(allow_singletons)
    , builder(H, treelet_table)  // NWSBuilder needs H + read-only table for C(T, v)
{
    if (!H || !treelet_list || !treelet_table || !output) {
        throw std::runtime_error("MultithreadNWSBuilder: null constructor argument");
    }
    if (nthreads == 0) {
        throw std::runtime_error("MultithreadNWSBuilder: nthreads must be >= 1");
    }

    // Seed the work queue with all singleton subtypes (one hyperedge).
    // Invariant required by NWSBuilder::build(): st.edges and st.verts are sorted.
    std::lock_guard<std::mutex> lk(work_mutex);

    for (Hypergraph::edge_t he = 0; he < H->number_of_hyperedges(); ++he) {
        const std::uint32_t sz = H->hyperedge_size(he);

        std::vector<Hypergraph::vertex_t> he_verts;
        he_verts.reserve(sz);
        for (std::uint32_t i = 0; i < sz; ++i) {
            he_verts.push_back(H->hyperedge_vertex(he, i));
        }
        std::sort(he_verts.begin(), he_verts.end());
        he_verts.erase(std::unique(he_verts.begin(), he_verts.end()), he_verts.end());

        // Initial weight is unused by NWSBuilder for the input subtype; set 0 for clarity.
        EdgeSubtype st{/*edges=*/{he}, /*verts=*/std::move(he_verts), /*weight=*/0};

        for (const auto& t : *treelet_list) {
            work_queue.emplace(t, st);
            visited[t].insert(st);  // ensure single enqueue per (t, st)
            ++tasks_in_flight;
        }
    }
}

void MultithreadNWSBuilder::build()
{
    const auto NV = H->number_of_vertices();

    // 1) Prepare per-thread state and launch workers
    std::vector<thread_state> states(nthreads);
    std::vector<std::thread>  threads;
    threads.reserve(nthreads);

    for (unsigned int i = 0; i < nthreads; ++i) {
        auto& m = states[i].nws_counts;
        m.reserve(treelet_list->size());
        for (const auto& t : *treelet_list) {
            m.emplace(t, std::vector<CountT>(NV, static_cast<CountT>(0)));
        }
        threads.emplace_back(&MultithreadNWSBuilder::worker_loop, this, i, &states[i]);
    }

    // 2) Wake up workers (if any were blocked before start)
    work_condvar.notify_all();

    // 3) Spin until the queue and tasks are drained
    //    (simple and robust; could be replaced with a completion condvar if needed)
    while (tasks_in_flight.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }

    // 4) Stop workers cleanly
    {
        std::lock_guard<std::mutex> lk(work_mutex);
        shutdown = true;
    }
    work_condvar.notify_all();

    for (auto& th : threads) th.join();

    // 5) Reduce per-thread partial counts into final map
    std::unordered_map<Treelet, std::vector<CountT>, Treelet::TreeletHash> final_counts;
    final_counts.reserve(treelet_list->size());

    for (unsigned th = 0; th < nthreads; ++th) {
        for (const auto& kv : states[th].nws_counts) {
            const Treelet&              t   = kv.first;
            const std::vector<CountT>&  src = kv.second;

            auto& dst = final_counts[t];
            if (dst.empty()) dst.assign(NV, static_cast<CountT>(0));
            assert(dst.size() == src.size());

            for (Hypergraph::vertex_t v = 0; v < NV; ++v) {
                dst[v] += src[v];
            }
        }
    }

    // 6) Serialize header and per-vertex rows
    const Hypergraph::vertex_t nv_hdr = H->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&nv_hdr), sizeof(nv_hdr));

    for (Hypergraph::vertex_t u = 0; u < NV; ++u) {
        // Collect (treelet, count) pairs, skipping zeros
        std::vector<std::pair<Treelet, std::uint64_t>> tbl;
        tbl.reserve(final_counts.size());

        for (const auto& kv : final_counts) {
            const Treelet&             t  = kv.first;
            const std::vector<CountT>& vv = kv.second;
            const CountT               sc = vv[u];

            // NWS accumulation must be non-negative before serialization
            assert(sc >= 0);
            if (sc != 0) {
                tbl.emplace_back(t, static_cast<std::uint64_t>(sc));
            }
        }

        // Use NWSBuilder helper to normalize/sort + pack (u, tbl) into a byte buffer
        auto [buf, bytes] = builder.to_normalized_sorted_byte_array(u, tbl);
        output->write(buf, static_cast<std::streamsize>(bytes));
        delete[] buf;
    }

    output->flush();
}

void MultithreadNWSBuilder::worker_loop(unsigned /*thread_id*/, thread_state* state)
{
    for (;;) {
        std::pair<Treelet, EdgeSubtype> work;

        // Acquire a task or exit on shutdown
        {
            std::unique_lock<std::mutex> lk(work_mutex);
            work_condvar.wait(lk, [&] { return shutdown || !work_queue.empty(); });

            if (shutdown && work_queue.empty()) {
                return; // graceful exit
            }

            work = std::move(work_queue.front());
            work_queue.pop();
        }

        // One task less in flight (we just consumed it)
        tasks_in_flight.fetch_sub(1, std::memory_order_acq_rel);

        const Treelet  t  = work.first;
        EdgeSubtype    st = std::move(work.second);

        // Per-thread accumulator for this treelet
        auto& counts = state->nws_counts[t];

        // Run one NWS step and enumerate all distinct one-edge extensions
        auto next = builder.build(st, t, counts);

        // For each extension, deduplicate globally (per treelet) and enqueue
        for (auto& st_next : next) {
            bool do_enqueue = false;
            {
                std::lock_guard<std::mutex> vg(visited_mutex);
                auto [_, inserted] = visited[t].insert(st_next);
                do_enqueue = inserted;
            }

            if (do_enqueue) {
                {
                    std::lock_guard<std::mutex> lk(work_mutex);
                    work_queue.emplace(t, std::move(st_next));
                }
                tasks_in_flight.fetch_add(1, std::memory_order_acq_rel);
                work_condvar.notify_one();
            }
        }
    }
}