#include "HyperOccurrenceSampler.h"

#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <vector>
#include <thread>
#include <algorithm> // for sort/unique

// per-sample hyperedge dedup without hash tables.
// We mark edges seen in the current sample using a monotonically increasing epoch.
namespace {
    thread_local std::vector<uint32_t> tls_seen_edge_build;
    thread_local uint32_t              tls_seen_epoch = 1;
}

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

// Single-pass builder for Gaifman + incidence (VxE) on U.
// - Visits each hyperedge touching U **once** (TLS epoch-based dedup).
// - Builds Gaifman bits by adding all pairs inside e∩U.
// - Builds a set of distinct masks over k vertices, then converts to a dense matrix.
void HyperOccurrenceSampler::build_weak_and_incidence(const UndirectedGraph::vertex_t* U,
                                                      unsigned k,
                                                      uint8_t* out_bits,
                                                      std::vector<std::vector<uint8_t>>& M)
{
    assert(k <= 16);

    // TLS mark-array to deduplicate visited hyperedges within the current sample.
    const uint32_t Medges = H->number_of_hyperedges();
    if (tls_seen_edge_build.size() != Medges)
        tls_seen_edge_build.assign(Medges, 0);
    const uint32_t mark = ++tls_seen_epoch;
    if (tls_seen_epoch == 0) { // wrap-around safety
        std::fill(tls_seen_edge_build.begin(), tls_seen_edge_build.end(), 0);
        tls_seen_epoch = 1;
    }

    // We accumulate column masks (k-bit) and deduplicate later by sort+unique.
    std::vector<uint32_t> masks;
    masks.reserve(32);

    // For each vertex in U, visit incident hyperedges once.
    for (unsigned iu = 0; iu < k; ++iu) {
        const auto v  = U[iu];
        const uint32_t dv = H->vertex_degree(v);
        for (uint32_t t = 0; t < dv; ++t) {
            const uint32_t e = H->incident_hyperedge(v, t);
            if (tls_seen_edge_build[e] == mark) continue; // already processed this edge
            tls_seen_edge_build[e] = mark;

            // Compute e ∩ U with a tiny O(k) membership check (k<=16).
            // We also keep the local indices to emit Gaifman pairs immediately.
            unsigned  local_idx[16];
            unsigned  m = 0;
            uint32_t  mask = 0;

            const uint32_t sz = H->hyperedge_size(e);
            for (uint32_t j = 0; j < sz; ++j) {
                const auto w = H->hyperedge_vertex(e, j);
                for (unsigned i = 0; i < k; ++i) {
                    if (w == U[i]) {
                        local_idx[m++] = i;
                        mask |= (1u << i);
                        break;
                    }
                }
            }

            if (m >= 2) {
                // Gaifman: add all pairs among vertices in e∩U.
                for (unsigned a = 1; a < m; ++a)
                    for (unsigned b = 0; b < a; ++b)
                        set_edge_bit(out_bits, local_idx[a], local_idx[b]);
                // Incidence: record the column mask for e∩U.
                masks.push_back(mask);
            }
        }
    }

    // Deduplicate and sort masks for deterministic columns.
    if (!masks.empty()) {
        std::sort(masks.begin(), masks.end());
        masks.erase(std::unique(masks.begin(), masks.end()), masks.end());
    }

    // Convert masks to a dense 0/1 matrix M of shape k x b.
    const uint16_t b = static_cast<uint16_t>(masks.size());
    M.assign(k, std::vector<uint8_t>(b, 0));
    for (uint16_t j = 0; j < b; ++j) {
        uint32_t mask = masks[j];
        for (uint16_t i = 0; i < k; ++i)
            if (mask & (1u << i)) M[i][j] = 1;
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