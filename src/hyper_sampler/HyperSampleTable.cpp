// MIT License
#include "HyperSampleTable.h"

#include <thread>
#include <memory>
#include <cstring>
#include <iomanip>
#include <cmath>

#include "../sampler/SpanningTreeCounter.h"   // contatore spanning trees
#include "../sampler/Occurrence.h"            // Occurrence classica (per Gaifman)

// ---- confronto footprint bipartito canonico ----
static inline int cmp_bip(const HyperOccurrence& a, const HyperOccurrence& b) {
    const size_t na = a.bipartite_binary_size();
    const size_t nb = b.bipartite_binary_size();
    if (na != nb) return (na < nb) ? -1 : 1;
    if (na == 0)  return 0;
    const int c = std::memcmp(a.bipartite_binary(), b.bipartite_binary(), na);
    return (c < 0) ? -1 : (c > 0 ? 1 : 0);
}

// =================== stime ===================

void HyperSampleTable::estimate_occurrences(double num_graph_treelets)
{
    if (num_samples == 0 || num_graph_treelets <= 0) {
        for (auto &e : entries) e.estimated_graph_occurrences = 0.0;
        return;
    }
    for (auto &e : entries) {
        if (e.num_spanning_trees == 0) {
            e.estimated_graph_occurrences = 0.0; // evita divisioni per zero
        } else {
            e.estimated_graph_occurrences =
                (static_cast<double>(e.sample_count) / static_cast<double>(num_samples))
                * (num_graph_treelets / static_cast<double>(e.num_spanning_trees));
        }
    }
}

void HyperSampleTable::estimate_frequencies()
{
    double tot = 0.0;
    for (const auto &e : entries) tot += e.estimated_graph_occurrences;
    if (tot <= 0) {
        for (auto &e : entries) e.estimated_graph_frequency = 0.0;
    } else {
        for (auto &e : entries)
            e.estimated_graph_frequency = e.estimated_graph_occurrences / tot;
    }
}

void HyperSampleTable::rescale_occurrences(double factor)
{
    if (std::fabs(factor - 1.0) < 1e-15) return;
    for (auto &e : entries) e.estimated_graph_occurrences *= factor;
}

// =================== ordinamenti / grouping ===================

void HyperSampleTable::sort_by_footprint()
{
    std::sort(entries.begin(), entries.end(),
              [](const Entry& x, const Entry& y){
                  return cmp_bip(x.occurrence, y.occurrence) < 0;
              });
}

void HyperSampleTable::group_by_footprint()
{
    if (entries.empty()) return;

    auto it = entries.begin();
    auto out = it;

    while (++it != entries.end()) {
        if (cmp_bip(out->occurrence, it->occurrence) == 0) {
            out->sample_count += it->sample_count;
            // NB: spanning_trees / stime si ricalcolano dopo
        } else {
            if (++out != it) *out = *it; // Entry è triviale
        }
    }
    entries.erase(++out, entries.end());
}

void HyperSampleTable::sort_by_estimate_occurrences()
{
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b){
                  return a.estimated_graph_occurrences > b.estimated_graph_occurrences;
              });
}

// =================== spanning trees (su Gaifman) ===================

void HyperSampleTable::spanning_tree_count_thread(
    const std::vector<std::vector<Entry>::iterator>& distinct,
    sequencer_t& sequencer,
    SpanningTreeCounter& counter,
    unsigned int gaifman_k)
{
    while (true) {
        auto batch = sequencer.next_batch();
        if (batch.from >= batch.to_exclusive) break;

        for (uint64_t i = batch.from; i < batch.to_exclusive; ++i) {
            auto it = distinct[i];

            // Costruisci Occurrence classica dal Gaifman footprint (k <= 16)
            const auto& ho = it->occurrence;
            Occurrence occ_gaif(gaifman_k, ho.vertices(), ho.gaifman().bytes);

            const uint64_t c = counter.number_of_rooted_spanning_trees(occ_gaif);

            // Propaga a tutte le entries con stesso footprint bipartito
            auto it2 = it;
            const auto it2_end = distinct[i+1];
            for (; it2 != it2_end; ++it2) it2->num_spanning_trees = c;
        }
    }
}

void HyperSampleTable::count_rooted_spanning_trees(const TreeletStructureSelector* selector,
                                                   unsigned int nthreads)
{
    if (entries.empty()) return;

#ifndef NDEBUG
    const unsigned int k = entries[0].occurrence.k();
    for (const auto& e : entries) { (void)e; assert(e.occurrence.k() == k); }
#else
    const unsigned int k = entries[0].occurrence.k();
#endif

    SpanningTreeCounter counter(k, selector);

    if (nthreads <= 1) {
        // sequenziale con deduplicazione su footprint bipartito
        auto it = entries.begin();
        while (it != entries.end()) {
            Occurrence occ_gaif(k, it->occurrence.vertices(), it->occurrence.gaifman().bytes);
            uint64_t c = counter.number_of_rooted_spanning_trees(occ_gaif);
            it->num_spanning_trees = c;

            ++it;
            while (it != entries.end() &&
                   cmp_bip((it-1)->occurrence, it->occurrence) == 0) {
                it->num_spanning_trees = c;
                ++it;
            }
        }
        return;
    }

    // multi-thread: individua gli head dei blocchi (footprint bipartito distinto)
    std::vector<std::vector<Entry>::iterator> distinct;
    distinct.reserve(entries.size() + 1);

    auto it = entries.begin();
    distinct.push_back(it);
    ++it;
    for (; it != entries.end(); ++it)
        if (cmp_bip((it-1)->occurrence, it->occurrence) != 0)
            distinct.push_back(it);
    distinct.push_back(entries.end()); // sentinel

    sequencer_t seq(0, distinct.size()-1, nthreads);
    auto threads = std::make_unique<std::thread[]>(nthreads);
    for (unsigned t=0; t<nthreads; ++t) {
        threads[t] = std::thread([this, &distinct, &seq, &counter, k]() {
            spanning_tree_count_thread(distinct, seq, counter, k);
        });
    }
    for (unsigned t=0; t<nthreads; ++t) threads[t].join();
}

// =================== norm2 & stampa ===================

double HyperSampleTable::norm2() const
{
    double acc = 0.0;
    for (const auto& e : entries) {
        const double s = static_cast<double>(e.sample_count);
        acc += s * s;
    }
    return (num_samples == 0) ? 0.0 : std::sqrt(acc) / static_cast<double>(num_samples);
}

std::ostream& operator<<(std::ostream& os, const HyperSampleTable& st)
{
    std::ios_base::fmtflags flags(os.flags());
    os << std::scientific << std::setprecision(4) << std::setfill('0');

    for (const auto& e : st.entries) {
        os << e.occurrence.bipartite_text()
           << ", " << e.estimated_graph_occurrences
           << ", " << e.estimated_graph_frequency
           << ", " << e.sample_count
           << ", " << e.type
           << ", " << e.num_spanning_trees << ",";

        // lista vertici
        for (unsigned i=0; i<e.occurrence.k(); ++i)
            os << " " << e.occurrence.vertices()[i];
        os << "\n";
    }

    os.flags(flags);
    return os;
}