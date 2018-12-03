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
#include "../common/graph/SimpleGraph.h"
#include "../common/RangeSampler.h"
#include "../common/ValueSortedMap.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletSelector.h"
#include "../common/CachedSTC.h"
#include "SampleTable.h"

class AdaptiveSampler {

	typedef google::dense_hash_map<Occurrence, std::pair<int, double>,
			Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_pair_table_t;
//typedef std::map<Occurrence, int, Occurrence::OccurrenceFootprintLess> occ_count_table_t;
	typedef google::dense_hash_map<Occurrence, int, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_count_table_t;
	typedef google::dense_hash_set<Occurrence, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_set_t;
	typedef google::dense_hash_map<Treelet, uint64_t, Treelet::TreeletHash, Treelet::compare_eq> treelet_uint64_table_t;

public:

	typedef google::dense_hash_set<Treelet, Treelet::TreeletHash, Treelet::compare_eq> treelet_set_t;

	/**
	 * It represents a treelet with all its possible rootings.
	 */
	class TreeletClass {
		Treelet representant = Treelet::invalid_treelet;
		treelet_set_t all;
	public:
		TreeletClass() {
			all.set_empty_key(Treelet::invalid_treelet);
		}
		TreeletClass(Treelet repr) {
			all.set_empty_key(Treelet::invalid_treelet);
			representant = repr;
			SimpleGraph::from_treelet(repr).decompose(&all, -1, true);
		}
		treelet_set_t &get_all() {
			return all;
		}
		size_t size() const {
			return all.size();
		}
	};

private:
	constexpr static unsigned int suffSamples = 1000;

	std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> numTreelets; // as computed by the build
	TreeletTable::treelet_count_t totTreelets = 0; // the sum of the map values above
	std::map<Treelet, double, Treelet::compare_less> treeletInefficiency; // estimated probability of yielding a graphlet in completedGraphlets
	ValueSortedMap<Treelet, double> treeletPriority; // function of efficiency, we always take the highest value
	Treelet currentTreelet; // the treelet in use for sampling
	google::dense_hash_map<Treelet, TreeletClass, Treelet::TreeletHash, Treelet::compare_eq> treeletClassMap; // each treelet has many rooted versions, here in a class mapped by a representant
	std::map<Treelet, Treelet, Treelet::compare_less> treeletToRepresentant; // each treelet mapped to its representant, so treeletClassMap[treeletToRepresentant[t]].all() contains t
	unsigned int size;
	UndirectedGraph* g;
	CachedSTC spTreeCounter;
	int totTreeletSwitches = 0;
	std::set<Occurrence, Occurrence::OccurrenceFootprintLess> completedGraphlets; // graphlets sampled at least suffSamples times
	treelet_uint64_table_t treeletSamples; // how many time each treelet has been used
	int totSamples; //FIXME: Needed?
	TreeletSelector *treeletSelector = nullptr;
	TreeletTableCollection *ttc;
	OccurrenceSampler* sampler = nullptr;
	double totManagementTime = 0;
	bool store_only_on_0 = false;
	double joinTime = 0, mergeTime = 0, weightsTime = 0, effTime = 0, prioTime = 0, samplerTime = 0, totTime = 0;

	void do_sample_mt(int num_samples, occ_count_table_t* counts, Random *rng, CachedSTC *stc = nullptr);

	void updateSampler();

public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, std::string dtzFile,
			std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> *counts,
			unsigned int size, TreeletTableCollection* ttc, bool store_only_on_0);

	/**
	 * Take samples and return a table with counts.
	 * n_samples = 0 means no limit on sample numbers, but only on the time budget.
	 */
	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random* rng, double time_budget = std::numeric_limits<double>::infinity());

	void recomputeTreeletPriorities(occ_pair_table_t&);

	inline double getUpdateTime() //FIXME: Do we need this?
	{
		return totManagementTime;
	}
};

#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
