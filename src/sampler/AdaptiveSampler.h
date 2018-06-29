/*
 * AdaptiveSampler.h
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_ADAPTIVESAMPLER_H_
#define SRC_SAMPLER_ADAPTIVESAMPLER_H_

#include <google/dense_hash_map>
#include <map>
#include <set>

#include "Occurrence.h"
#include "OccurrenceSampler.h"
#include "sampler_opts.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/RangeSampler.h"
#include "../common/ValueSortedMap.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletSelector.h"
#include "../common/CachedSTC.h"
#include "SampleTable.h"

class AdaptiveSampler {
private:
	sampler_opts opts;
	std::map<Treelet, TreeletTable::treelet_count_t> treeletCounts;
	ValueSortedMap<Treelet, double> residuals;
	unsigned int size;
	UndirectedGraph* g;
	Random* rng;
	CachedSTC stc;
	std::set<Occurrence, Occurrence::OccurrenceCompare> S;
	std::map<Occurrence, unsigned int, Occurrence::OccurrenceCompare> counts;
	std::map<Occurrence, double, Occurrence::OccurrenceCompare> weights;
	unsigned int totSamples;
	unsigned int c = 500;
	Treelet currentTreelet;
	TreeletSelector *treeletSelector = nullptr;
	TreeletTableCollection *ttc;
	OccurrenceSampler* sampler = nullptr;
	void updateSampler();
	double totUpdateTime = 0;
public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, std::string dtzFile,
			std::map<Treelet, TreeletTable::treelet_count_t> *counts, unsigned int size,
			Random* rng, TreeletTableCollection* ttc, sampler_opts opts);
	~AdaptiveSampler();
	OccurrenceSampler::table_t* create_table();
	void sample_one(Occurrence* occurrence);
	std::map<Occurrence, std::pair<int, double>, Occurrence::OccurrenceCompare> sample_many(
			int num_samples);
	SampleTable sample(int n_samples, int number_of_threads);
	inline double getUpdateTime() {
		return totUpdateTime;
	}
};

#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
