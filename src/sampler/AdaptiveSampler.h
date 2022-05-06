// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SRC_SAMPLER_ADAPTIVESAMPLER_H_
#define SRC_SAMPLER_ADAPTIVESAMPLER_H_

#include "Occurrence.h"
#include <dense_hash_map>
#include <dense_hash_set>
#include <map>
#include <unordered_set>

#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "OccurrenceSampler.h"
#include "SampleTable.h"
#include "ValueSortedMap.h"

class AdaptiveSampler
{
private:
	struct occurrent_info_t
	{
		uint64_t num_occurrences = 0;
		double weight = 0;
	};

	typedef google::dense_hash_map<Occurrence, occurrent_info_t, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_info_table_t;

	constexpr static unsigned int suffSamples = 1000;

	const UndirectedGraph* graph;
    const TreeletTableCollection *ttc;
    const unsigned int size;
	const unsigned int number_of_threads;
	const bool store_only_on_0 = false;

	const uint32_t buffer_size;
	const UndirectedGraph::vertex_t buffer_degree;

	std::map<Treelet::treelet_structure_t, TreeletTable::treelet_count_t> numTreelets; // as computed by the build
	TreeletTable::treelet_count_t totTreelets = 0; // the sum of the map values above
	ValueSortedMap<Treelet::treelet_structure_t, double> treeletPriority; // function of efficiency, we always take the highest value
	Treelet::treelet_structure_t current_treelet_structure; // the treelet in use for sampling

	std::map<Treelet::treelet_structure_t , Treelet::treelet_structure_t > structure_to_representant; // each treelet structure is mapped to its representant
    std::map<Treelet::treelet_structure_t , std::vector<Treelet::treelet_structure_t>> representant_to_structures; //maps each representant to all the treelets structures it represents //FIXME: std::vector or something else?

    std::map<Treelet::treelet_structure_t, std::vector<Occurrence> > representant_to_containing_occurrences; //maps a representant structure S to all occurrences that are spanned by treelet represented by S
    std::unordered_map<Occurrence, std::map<Treelet::treelet_structure_t, uint64_t >, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality > occurrences_to_spanning_representants; //maps an occurrence occ to the representants of the treelets of occ, along with the number of occurrences of their represented treelets //FIXME: type?

    std::set<Occurrence, Occurrence::OccurrenceFootprintLess> completedGraphlets; // graphlets sampled at least suffSamples times

	uint64_t totTreeletSwitches = 0;
	TreeletStructureSelector *treeletSelector = nullptr;
	OccurrenceSampler* sampler = nullptr;

	double totManagementTime = 0;
	double sampleTime = 0, mergeTime = 0, weightsTime = 0, effTime = 0, prioTime = 0, updateTime = 0, totTime = 0;

	void update_sampler();

public:
	/**
	 * Build an adaptive sampler.
	 */
	AdaptiveSampler(UndirectedGraph* g, TreeletTableCollection* ttc, unsigned int size, unsigned int numbber_of_threads, bool store_only_on_0, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree);

	/**
	 * Take samples and return a table with counts.
	 * n_samples = 0 means no limit on sample numbers, but only on the time budget.
	 */
	SampleTable* sample(uint64_t n_samples, Random* rng, double time_budget = std::numeric_limits<double>::infinity(), const TreeletStructureSelector* build_selector=nullptr);

	void recomputeTreeletPriorities(occ_info_table_t&);
};

#endif /* SRC_SAMPLER_ADAPTIVESAMPLER_H_ */
