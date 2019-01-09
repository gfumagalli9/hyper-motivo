/*
 * OccurrenceStarSampler.h
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_
#define SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_

#include "Occurrence.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/AliasMethodSampler.h"
#include "../common/sequencer/DynamicSequencer.h"
#include <sparsehash/dense_hash_map>

class SampleTable;

class OccurrenceStarSampler
{
public:
    typedef DynamicSequencer<uint64_t> sequencer_t;
	typedef google::dense_hash_map<Occurrence, int, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_count_table_t;

private:
    static constexpr UndirectedGraph::vertex_t sampling_vs_shuffling_degree_threshold = 1024;

	const UndirectedGraph *g; // the host graph
    const unsigned int size; // k, the size of the stars
	const bool canonicize; // whether to canonicalize the occurrences
    unsigned int number_of_threads;

    AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>* root_sampler = nullptr;

	void sample_one(Occurrence* occurrence, Random* rng);
    void do_sample_mt(occ_count_table_t* tab, sequencer_t *sequencer, Random *rng);


public:
    OccurrenceStarSampler(const UndirectedGraph *g, unsigned int size, unsigned int number_of_threads, bool canonicize);
    ~OccurrenceStarSampler();

    uint128_t number_of_stars() const
	{
		return root_sampler->get_total_weight();
	}

//    Occurrence* sample(uint64_t num_samples, Random *rng);
    SampleTable* sample(uint64_t num_samples, Random *rng, double time_budget = std::numeric_limits<double>::infinity());
};

#endif /* SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_ */
