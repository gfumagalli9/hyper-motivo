/*
 * OccurrenceStarSampler.h
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_
#define SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_

#include <google/dense_hash_map>
#include <map>

#include "../common/graph/UndirectedGraph.h"
#include "../common/RangeSampler.h"
#include "Occurrence.h"
#include "OccurrenceSampler.h"
#include "../common/sequencer/DynamicSequencer.h"
#include "../common/io/ConcurrentWriter.h"
#include "../common/DiscreteDistribution.h"

class OccurrenceStarSampler {
private:
	UndirectedGraph *g; // the host graph
	unsigned int size; // k, the size of the stars
	Random *rng; // random number generator
	bool canonicize; // whether to canonicalize the occurrences
	bool no_rejection; // if true, keep all occurrences; if false, use rejection sampling
	bool group_same; // group by isomorphism class
//	RangeSampler<UndirectedGraph::vertex_t>* root_sampler;
	DiscreteDistribution* root_sampler = nullptr;
public:
	OccurrenceStarSampler(UndirectedGraph* g, unsigned int size, Random* rng, bool canonicize,
			bool no_rejection, bool group_same);
	~OccurrenceStarSampler();
	void sample_one(Occurrence* occurrence, UndirectedGraph::vertex_t root = -1);
	void sample_many(OccurrenceSampler::table_t* count_table, int nsamples);
	OccurrenceSampler::table_t* create_table();
	OccurrenceSampler::table_t* sample(int nsamples, int nthreads);
	inline DiscreteDistribution* get_root_sampler() const {
		return root_sampler;
	}
};

#endif /* SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_ */
