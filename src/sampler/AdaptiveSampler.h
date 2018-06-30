/*
 * AdaptiveSampler.h
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_ADAPTIVESAMPLER_H_
#define SRC_SAMPLER_ADAPTIVESAMPLER_H_

#include <google/dense_hash_map>
#include <google/dense_hash_set>
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
	std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> numTreelets; // as computed by the build
	TreeletTable::treelet_count_t totTreelets = 0; // the sum of the map values above
	std::map<Treelet, double, Treelet::compare_less> treeletInefficiency; // estimated probability of yielding a graphlet in completedGraphlets
	ValueSortedMap<Treelet, double> treeletPriority; // function of efficiency, we always take the highest value
	Treelet currentTreelet; // the treelet in use for sampling
	unsigned int size;
	UndirectedGraph* g;
	Random* rng;
	CachedSTC spTreeCounter;
	int suffSamples = 1000;
	std::set<Occurrence, Occurrence::compare_less> completedGraphlets; // graphlets sampled at least suffSamples times
	std::map<Treelet, unsigned int, Treelet::compare_less> treeletSamples; // how many time each treelet has been used
	std::map<Occurrence, unsigned int, Occurrence::compare_less> graphletCount; // how many times each graphlet has been sampled
	std::map<Occurrence, double, Occurrence::compare_less> graphletWeight; // weights (it's complicate)
	int totSamples;
	TreeletSelector *treeletSelector = nullptr;
	TreeletTableCollection *ttc;
	OccurrenceSampler* sampler = nullptr;
	double totUpdateTime = 0;
	void updateSampler();
public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, std::string dtzFile,
			std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> *counts,
			unsigned int size, Random* rng, TreeletTableCollection* ttc, sampler_opts opts);
	~AdaptiveSampler();
	OccurrenceSampler::table_t* create_table();
	void sample_one(Occurrence* occurrence);
	void sample_st(int num_samples,
			std::map<Occurrence, std::pair<int, double>, Occurrence::compare_less>* count_table);
	void just_sample(int num_samples, std::map<Occurrence, int, Occurrence::compare_less>* counts);
	SampleTable sample(int n_samples, int number_of_threads);
	inline double getUpdateTime() {
		return totUpdateTime;
	}
};

#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
