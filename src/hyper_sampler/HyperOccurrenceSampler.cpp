#include "HyperOccurrenceSampler.h"

#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <vector>
#include <thread>

// Worker thread: pull batches from the sequencer and append samples.
// Terminates either when there is no work left (finite mode) or when the
// time budget expires and terminate_flag is set (infinite mode).
void HyperOccurrenceSampler::sample_thread(unsigned int thread_no,
                                           std::vector<HyperOccurrence>& out_samples,
                                           sequencer_t* sequencer,
                                           Random* rng,
                                           TimeoutThreadSync& sync)
{
    auto& terminate_flag = sync.get_termination_flag(thread_no);

    while (true) {
        const typename sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if (batch.from >= batch.to_exclusive) break; // no more work

        for (uint64_t i = batch.from; i < batch.to_exclusive; ++i) {
            if (terminate_flag) {
                sync.signal_termination_one();
                return;
            }

            HyperOccurrence occ;
            sample_one(&occ, rng);
            out_samples.emplace_back(std::move(occ));
        }
    }

    // Finished naturally (no more batches)
    sync.signal_termination_one();
}

// Build Gaifman bits of the weakly-induced subhypergraph on U.
// For each hyperedge e incident to at least one vertex in U, consider e' = e ∩ U.
// For every pair in e' add an (undirected) Gaifman edge bit.
void HyperOccurrenceSampler::build_weak_induced(const UndirectedGraph::vertex_t* U,
                                                unsigned k,
                                                uint8_t* out_bits)
{
    assert(k <= 16);

    // Map vertex id -> position in U[0..k-1]
    std::unordered_map<UndirectedGraph::vertex_t, int> index_of;
    index_of.reserve(k * 2);
    for (unsigned i = 0; i < k; ++i)
        index_of.emplace(U[i], static_cast<int>(i));

    // Collect each hyperedge incident to U exactly once
    std::vector<uint32_t> edges_to_visit;
    edges_to_visit.reserve(64);

    std::unordered_set<uint32_t> seen_edges;
    seen_edges.reserve(64);

    for (unsigned i = 0; i < k; ++i) {
        const auto v  = U[i];
        const uint32_t dv = H->vertex_degree(v);
        for (uint32_t t = 0; t < dv; ++t) {
            const uint32_t e = H->incident_hyperedge(v, t);
            if (seen_edges.insert(e).second)
                edges_to_visit.push_back(e);
        }
    }

    // For each edge, add all pairs in (e ∩ U) to Gaifman bits
    unsigned local_idx[16];
    for (uint32_t e : edges_to_visit) {
        unsigned m = 0;
        const uint32_t sz = H->hyperedge_size(e);
        for (uint32_t j = 0; j < sz; ++j) {
            const auto w = H->hyperedge_vertex(e, j);
            auto it = index_of.find(w);
            if (it != index_of.end())
                local_idx[m++] = static_cast<unsigned>(it->second);
        }
        for (unsigned a = 1; a < m; ++a)
            for (unsigned b = 0; b < a; ++b)
                set_edge_bit(out_bits, local_idx[a], local_idx[b]);
    }
}

HyperSampleTable* HyperOccurrenceSampler::sample(const uint64_t num_samples,
                                                 unsigned int number_of_threads,
                                                 Random* rng,
                                                 double time_budget)
{
    // Nothing to do if the request is ill-posed (NaN/<=0 budget AND num_samples==0).
    if (std::isnan(time_budget) || time_budget <= 0 || (num_samples == 0 && std::isinf(time_budget)))
        return new HyperSampleTable();

    // Avoid oversubscribing threads for very small workloads.
    if (num_samples != 0 && num_samples < 10 * number_of_threads)
        number_of_threads = static_cast<unsigned>((num_samples + 9) / 10);

    TimeoutThreadSync threadSync(number_of_threads);

    // Per-thread buffers to avoid contention.
    std::vector<std::vector<HyperOccurrence>> per_thread_samples(number_of_threads);

    if (num_samples != 0) {
        const uint64_t per_thread =
            (num_samples + number_of_threads - 1) / number_of_threads;
        for (auto& v : per_thread_samples)
            v.reserve(static_cast<size_t>(per_thread));
    }

    // Independent RNGs per thread.
    std::vector<Random*> rngs(number_of_threads, nullptr);
    for (unsigned i = 0; i < number_of_threads; ++i)
        rngs[i] = rng->derived_rng();

    // Launch workers.
    sequencer_t sequencer(
        /*from=*/0,
        /*to=*/(num_samples != 0) ? num_samples : sequencer_t::to_max,
        number_of_threads);

    std::vector<std::thread> threads;
    threads.reserve(number_of_threads);
    for (unsigned i = 0; i < number_of_threads; ++i) {
        threads.emplace_back([this, i, &per_thread_samples, &sequencer, &rngs, &threadSync] {
            sample_thread(i, per_thread_samples[i], &sequencer, rngs[i], threadSync);
        });
    }

    // Wait for completion or time budget expiry.
    if (!std::isinf(time_budget)) threadSync.wait_timeout(time_budget);
    else                          threadSync.wait();

    threadSync.request_termination();
    for (auto& th : threads) th.join();

    // Consolidate samples into a single table.
    auto* table = new HyperSampleTable();
    for (unsigned i = 0; i < number_of_threads; ++i) {
        table->add_occurrences(per_thread_samples[i].begin(),
                               per_thread_samples[i].end(),
                               'H'); // 'H' = hyper
        per_thread_samples[i].clear();
    }

    // Cleanup RNGs.
    for (auto* prng : rngs) delete prng;

    return table;
}