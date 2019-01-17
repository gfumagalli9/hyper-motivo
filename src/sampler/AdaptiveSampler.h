/*
 * AdaptiveSampler.h
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_ADAPTIVESAMPLER_H_
#define SRC_SAMPLER_ADAPTIVESAMPLER_H_

#include "Occurrence.h"
#include <sparsehash/dense_hash_map>
#include <sparsehash/dense_hash_set>
#include <map>
#include <set>

#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/SimpleGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletSelector.h"
#include "CachedSTC.h"
#include "OccurrenceSampler.h"
#include "SampleTable.h"
#include "ValueSortedMap.h"

class AdaptiveSampler
{
private:
	struct occurrent_info_t
	{
		int num_occurrences = 0;
		double weight = 0;
	};

	typedef google::dense_hash_map<Occurrence, occurrent_info_t, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_pair_table_t;
	typedef google::dense_hash_map<Occurrence, int, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_count_table_t;
	typedef google::dense_hash_set<Occurrence, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_set_t;
	typedef google::dense_hash_map<Treelet, uint64_t, Treelet::TreeletHash> treelet_uint64_table_t;
	typedef google::dense_hash_set<Treelet, Treelet::TreeletHash> treelet_set_t;

	/**
	 * It represents a treelet with all its possible rootings.
	 */
	class TreeletClass
	{
	private:
		treelet_set_t all;

	public:
		TreeletClass()
		{
			all.set_empty_key(invalid_treelet);
		}

		explicit TreeletClass(Treelet repr)
		{
			all.set_empty_key(invalid_treelet);
			SimpleGraph::from_treelet(repr).decompose(&all, -1, true);
		}

		const treelet_set_t& get_all() const { return all; }

		//See remark in builder/ColorCodingHashmap.h
		unsigned long size() const { return all.size(); }
	};

	constexpr static unsigned int suffSamples = 1000;

	const UndirectedGraph* graph;
	const unsigned int size;
	const TreeletTableCollection *ttc;
	const bool store_only_on_0 = false;

	std::map<Treelet, TreeletTable::treelet_count_t> numTreelets; // as computed by the build
	TreeletTable::treelet_count_t totTreelets = 0; // the sum of the map values above
	ValueSortedMap<Treelet, double> treeletPriority; // function of efficiency, we always take the highest value
	Treelet currentTreelet; // the treelet in use for sampling
	google::dense_hash_map<Treelet, TreeletClass, Treelet::TreeletHash> treeletClassMap; // each treelet has many rooted versions, here in a class mapped by a representant
	std::map<Treelet, Treelet> treeletToRepresentant; // each treelet mapped to its representant, so treeletClassMap[treeletToRepresentant[t]].all() contains t
    std::set<Occurrence, Occurrence::OccurrenceFootprintLess> completedGraphlets; // graphlets sampled at least suffSamples times
    treelet_uint64_table_t treeletSamples; // how many time each treelet has been used

	CachedSTC spTreeCounter;
	int totTreeletSwitches = 0;
	TreeletSelector *treeletSelector = nullptr;
	OccurrenceSampler* sampler = nullptr;

	double totManagementTime = 0;
	double joinTime = 0, mergeTime = 0, weightsTime = 0, effTime = 0, prioTime = 0, samplerTime = 0, totTime = 0;

	void do_sample_mt(uint64_t num_samples, occ_count_table_t* counts, Random *rng, CachedSTC *stc = nullptr);

	void update_sampler();

public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, unsigned int size, TreeletTableCollection* ttc, bool store_only_on_0);

	/**
	 * Take samples and return a table with counts.
	 * n_samples = 0 means no limit on sample numbers, but only on the time budget.
	 */
	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random* rng, double time_budget = std::numeric_limits<double>::infinity());

	void recomputeTreeletPriorities(occ_pair_table_t&);
};

#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
