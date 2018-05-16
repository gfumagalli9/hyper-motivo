/*
 * SpanningTreeCounter.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SPANNINGTREECOUNTER_H_
#define SRC_SAMPLER_SPANNINGTREECOUNTER_H_

#include "../common/treelets/TreeletSelector.h"
#include "../sampler/Occurrence.h"

/**
 * Counts the spanning trees of an occurrence via color-coding
 */
class SpanningTreeCounter {
public:
	SpanningTreeCounter();
	~SpanningTreeCounter();
	static uint64_t num_spanning_trees(Occurrence *occ);
	static uint64_t num_spanning_trees(Occurrence *occ, TreeletSelector *ts);
};

#endif /* SRC_SAMPLER_SPANNINGTREECOUNTER_H_ */
