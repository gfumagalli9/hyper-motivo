/*
 * OccurrenceStarSampler.h
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_
#define SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_

#include "../common/graph/UndirectedGraph.h"
#include "../common/random/AliasMethodSampler.h"
#include "Occurrence.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"
#include "TimeoutThreadSync.h"

class OccurrenceStarSampler
{
public:
    typedef DynamicSequencer<uint64_t> sequencer_t;

private:
    static constexpr UndirectedGraph::vertex_t sampling_vs_shuffling_degree_threshold = 1024;

	const UndirectedGraph *g; // the host graph
    const unsigned int size; // k, the size of the stars
	const bool canonicize; // whether to canonicalize the occurrences

    AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>* root_sampler = nullptr;

	void sample_one(Occurrence* occurrence, Random* rng);

	void sample_thread(unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync);

public:
    OccurrenceStarSampler(const UndirectedGraph *g, unsigned int size, bool canonicize);

    ~OccurrenceStarSampler();

    uint128_t number_of_stars() const
	{
		return root_sampler->get_total_weight();
	}

    SampleTable* sample(uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget);
};

#endif /* SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_ */
