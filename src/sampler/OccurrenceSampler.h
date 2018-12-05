//
// Created by steven on 9/11/17.
//

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include "../common/Occurrence.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/SpanningTreeCounter.h"
#include "TreeletSampler.h"
#include "../common/sequencer/DynamicSequencer.h"
#include <google/dense_hash_map>

class SampleTable;

class OccurrenceSampler {
public:
	typedef DynamicSequencer<uint64_t> sequencer_t;
	typedef google::dense_hash_map<Occurrence, int, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_count_table_t;

private:
	const UndirectedGraph *graph;
	TreeletTableCollection *ttc;
	const unsigned int size;

	const bool vertices;
	const bool graphlets;
	const bool canonicize;
	const bool no_rejection;
	bool delete_stc = true;

	TreeletSampler sampler;
	TreeletSelector *sp_counter_selector = nullptr;

	SpanningTreeCounter* stc;

	void do_sample_mt [[gnu::hot, gnu::flatten]] (occ_count_table_t* table, sequencer_t *sequencer, Random *rng);
public:
	inline void sample_one [[gnu::hot]] (Occurrence *occurrence, Random *rng);

	SampleTable* sample(const uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

    OccurrenceSampler(const UndirectedGraph *graph, TreeletTableCollection* ttc, unsigned int size,
                                         bool vertices, bool graphlets, bool canonicize, bool no_rejection) :
            graph(graph), ttc(ttc), size(size), vertices(vertices), graphlets(graphlets), canonicize(canonicize),
            no_rejection(no_rejection), sampler(graph, ttc, size)
    {
    	stc = new SpanningTreeCounter();
    }

    void setSpanningTreeCounter(SpanningTreeCounter* stc) {
    	if (this->stc) {
    		delete this->stc;
    		this->delete_stc = false;
    	}
    	this->stc = stc;
    }

    ~OccurrenceSampler() {
    	if (this->delete_stc)
    		delete stc;
    }

    /**
     * Set the treelet selector.
     * - the first specifies the treelets to be used for sampling the graphlets.
     * - the second specified the treelets to be used to count the spanning trees of
     *   the graphlets, including all their subtrees. This means that you want this
     *   second selector to contain all the treelets of the first, plus their subtrees,
     *   if the first selector is in INCLUDE mode. By default, it uses the first selector
     *   again, which is correct if in EXCLUDE mode.
     */
    void set_selector(const TreeletSelector *selector, unsigned int number_of_threads, const TreeletSelector *sp_counter_selector = nullptr);
};



void OccurrenceSampler::sample_one(Occurrence *occurrence, Random *rng)
{
    UndirectedGraph::vertex_t sampled_vertices[16];
    UndirectedGraph::vertex_t root = sampler.sample_root(rng);
    assert(root < graph->number_of_vertices());
    Treelet t = sampler.sample_treelet(root, rng);

    static thread_local OccurrenceCanonicizer canonicizer(size);


    while (true)
    {
        if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
        {
#ifndef NDEBUG
            bool success =
#endif
                    sampler.sample_rooted_occurrence(t, root, sampled_vertices, rng); //FIXME: Handle case in which there are no treelets
            assert(success);
        }

        if (graphlets)
        {
            new(occurrence) Occurrence(size, graph, sampled_vertices);

            if (!no_rejection && rng->random_uint<uint64_t>(0, occurrence->number_of_spanning_trees() - 1) != 0)
                continue; //Rejection
        }
        else
            new(occurrence) Occurrence(t, sampled_vertices);

        if(canonicize)
            canonicizer.canonicize(occurrence);

        break;
    }
}

#endif //MOTIVO_OCCURRENCESAMPLER_H
