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

class AdaptiveSampler
{
public:

    //FIXME: Can these classes be merged with the ones used by SampleTable? Can also we use pointers?
    struct OccurrenceFootprintHash
    {
        inline size_t operator()[[gnu::hot,gnu::flatten]] (const Occurrence &key) const
        {
            size_t seed;
            seed = key.is_valid()?0xcb7fedb03a45866f:0xb896186490f1c8e9;

            const char* p = key.binary_footprint();
            for(unsigned int i = 0; i < Occurrence::binary_footprint_bytes; i++)
                seed ^= static_cast<unsigned char>(p[i])*0xff51afd7ed558ccd +0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };

    struct OccurrenceFootprintEquality
    {
        inline bool operator() [[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
        {
            return (occ1.is_valid()==occ2.is_valid()) && !memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes);
        }
    };


    struct OcurrenceFootprintLess
    {
        inline bool operator()[[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
        {
            return (occ1.is_valid()==occ2.is_valid()) && (memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes) < 0);
        }
    };

private:
    constexpr static unsigned int suffSamples = 1000;

	std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> numTreelets; // as computed by the build
	TreeletTable::treelet_count_t totTreelets = 0; // the sum of the map values above
	std::map<Treelet, double, Treelet::compare_less> treeletInefficiency; // estimated probability of yielding a graphlet in completedGraphlets
	ValueSortedMap<Treelet, double> treeletPriority; // function of efficiency, we always take the highest value
	Treelet currentTreelet; // the treelet in use for sampling
	unsigned int size;
	UndirectedGraph* g;
	CachedSTC spTreeCounter;
	int totTreeletSwitches = 0;
	std::set<Occurrence, OcurrenceFootprintLess> completedGraphlets; // graphlets sampled at least suffSamples times
	std::map<Treelet, unsigned int, Treelet::compare_less> treeletSamples; // how many time each treelet has been used
	std::map<Occurrence, unsigned int, OcurrenceFootprintLess> graphletCount; // how many times each graphlet has been sampled
	std::map<Occurrence, double, OcurrenceFootprintLess> graphletWeight; // weights (it's complicate)
	int totSamples; //FIXME: Needed?
	TreeletSelector *treeletSelector = nullptr;
	TreeletTableCollection *ttc;
	OccurrenceSampler* sampler = nullptr;
	double totManagementTime = 0;


    void sample_st(int num_samples, std::map<Occurrence, std::pair<int, double>, OcurrenceFootprintLess>* count_table, Random* rng);
    void do_sample_mt(int num_samples, std::map<Occurrence, int, OcurrenceFootprintLess> *counts, Random *rng);

    void updateSampler();

public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, std::string dtzFile, std::map<Treelet, TreeletTable::treelet_count_t, Treelet::compare_less> *counts, unsigned int size, TreeletTableCollection* ttc);

	SampleTable sample(unsigned int n_samples, unsigned int number_of_threads, Random* rng);

	inline double getUpdateTime() //FIXME: Do we need this?
	{
		return totManagementTime;
	}
};


#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
