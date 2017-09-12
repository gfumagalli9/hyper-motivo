//
// Created by steven on 9/11/17.
//

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include "../common/graph/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "../common/sequencer/DynamicSequencer.h"
#include "../common/io/ConcurrentWriter.h"

class OccurrenceSampler
{
private:
    typedef DynamicSequencer<uint64_t> sequencer_t;

    static constexpr size_t buffer_size=1024*1024; //1MiB

    static constexpr unsigned int vertex_no_digits_ub = 1 + std::numeric_limits<uint64_t>::digits/3;
    static constexpr unsigned int spanning_tree_no_digits_ub = 1 + std::numeric_limits<UndirectedGraph::vertex_t>::digits/3;

    static constexpr unsigned int max_occurrence_size =
            Occurrence::text_footprint_bytes + 1 //text footprint
            + spanning_tree_no_digits_ub + 1     //spanning trees
            + 16 * (vertex_no_digits_ub + 1)     //verices
            + 1;                                 //newline

    UndirectedGraph *graph;
    TreeletTableCollection *ttc;
    const unsigned int size;
    const uint64_t num_samples;
    Random *rng;

    const bool vertices;
    const bool graphlets;
    const bool spanning_trees_no;
    const bool footprints;
    const bool canonicize;
    const bool no_rejection;
    const bool text;

    std::ostream *output;
    const unsigned int number_of_threads;

    TreeletSampler sampler;

    void do_sample_st [[gnu::hot, gnu::flatten]]  ();
    void do_sample_mt [[gnu::hot, gnu::flatten]] (sequencer_t *sequencer, ConcurrentWriter *writer);
    inline void sample_one [[gnu::hot]] (Occurrence *occurrence);

    char* write(Occurrence *occurrence, char* buf);

public:
    void sample();

    OccurrenceSampler(UndirectedGraph *graph, TreeletTableCollection* ttc, unsigned int size, uint64_t num_samples, Random *rng,
                      bool vertices, bool graphlets, bool spanning_trees_no, bool footprints, bool canonicize, bool no_rejection,
                      bool text, std::ostream *out, unsigned int number_of_threads)
            : graph(graph), ttc(ttc), size(size), num_samples(num_samples), rng(rng), vertices(vertices), graphlets(graphlets), spanning_trees_no(spanning_trees_no), footprints(footprints),
              canonicize(canonicize), no_rejection(no_rejection), text(text), output(out), number_of_threads(number_of_threads), sampler(graph, ttc, rng)
    {}

};


#endif //MOTIVO_OCCURRENCESAMPLER_H
