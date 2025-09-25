#pragma once
// MIT License
// HyperSampleTable — like SampleTable but grouping by canonical bipartite incidence

#include <vector>
#include <ostream>
#include <string>
#include <algorithm>
#include <cstdint>

#include "../common/treelets/TreeletStructureSelector.h"
#include "../sampler/DynamicSequencer.h"
#include "HyperOccurrence.h"

class SpanningTreeCounter; // fwd (def in sampler/SpanningTreeCounter.h)

class HyperSampleTable {
public:
    struct Entry {
        HyperOccurrence occurrence;         // bipartite canon + gaifman bits
        uint64_t num_spanning_trees = 0;    // computed on Gaifman
        uint64_t sample_count = 0;
        double   estimated_graph_frequency = 0;
        double   estimated_graph_occurrences = 0;
        char     type = '?';                // e.g., 'N','H','M','W'
    };

    using const_iterator = std::vector<Entry>::const_iterator;

    static constexpr const char* header =
        "hyper_motif, est_occurrences, est_frequency, samples, sampling_algo, spanning_trees, vertices";

private:
    std::vector<Entry> entries;
    uint64_t num_samples = 0;

    using sequencer_t = DynamicSequencer<uint64_t>;

    void spanning_tree_count_thread(const std::vector<std::vector<Entry>::iterator>& distinct,
                                    sequencer_t& sequencer,
                                    SpanningTreeCounter& counter,
                                    unsigned int gaifman_k);

public:
    HyperSampleTable() = default;

    inline void add_entry(Entry e) {
        entries.push_back(e);
        num_samples += e.sample_count;
    }

    template<typename It> void add_occurrences(It first, It last, char type) {
        for (auto it = first; it != last; ++it) {
            Entry e;
            e.occurrence   = *it;
            e.sample_count = 1;
            e.type         = type;
            add_entry(e);
        }
    }

    // Estimators (stessa logica di SampleTable)
    void estimate_occurrences(double num_graph_treelets);
    void estimate_frequencies();
    void rescale_occurrences(double factor);

    // Sorting / grouping by canonical bipartite footprint
    void sort_by_footprint();
    void group_by_footprint();
    void sort_by_estimate_occurrences();

    // Count rooted spanning trees on Gaifman (k <= 16)
    void count_rooted_spanning_trees(const TreeletStructureSelector* selector,
                                     unsigned int nthreads);

    // Convenience/IO
    inline uint64_t get_num_samples() const { return num_samples; }
    inline uint64_t size() const { return entries.size(); }

    inline const_iterator begin() const { return entries.cbegin(); }
    inline const_iterator end()   const { return entries.cend();   }

    double norm2() const;

    friend std::ostream& operator<<(std::ostream& os, const HyperSampleTable& st);
};