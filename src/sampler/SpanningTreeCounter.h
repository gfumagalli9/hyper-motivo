/*
 * SpanningTreeCounter.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SPANNINGTREECOUNTER_H_
#define SRC_SAMPLER_SPANNINGTREECOUNTER_H_

#include "Occurrence.h"
#include "../common/treelets/TreeletSelector.h"

/**
 * Counts the spanning trees of an occurrence via color-coding
 */
class SpanningTreeCounter
{
public:
	SpanningTreeCounter() = default;

	static uint64_t num_spanning_trees(const Occurrence &occ);
	static uint64_t  num_spanning_trees(const Occurrence &occ, const TreeletSelector *ts);
	static unsigned int  num_spanning_stars(const Occurrence &occ);
	static uint64_t num_spanning_trees_nostars(const Occurrence &occ);
};

#endif /* SRC_SAMPLER_SPANNINGTREECOUNTER_H_ */
