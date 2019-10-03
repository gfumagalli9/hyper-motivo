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

#include "AdaptiveSampler.h"
#include "OccurrenceSampler.h"
#include <thread>
#include <vector>
#include <queue>
#include <cmath>
#include "../common/util.h"
#include "ColorCodingSpanningTreeCounter.h"

AdaptiveSampler::AdaptiveSampler(UndirectedGraph* graph, TreeletTableCollection* ttc, unsigned int size,
		unsigned int number_of_threads, bool store_only_on_0, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree)
			: graph(graph), ttc(ttc), size(size), number_of_threads(number_of_threads), store_only_on_0(store_only_on_0),
			buffer_size(buffer_size), buffer_degree(buffer_degree)
{
	std::map<Treelet::treelet_structure_t, TreeletTable::treelet_count_t> numTreelets2;

	// Read the treelet counts
	TreeletTable *table = ttc->get_table(size);
	for (UndirectedGraph::vertex_t u = 0; u < table->number_of_vertices(); u++)
    {
		for(TreeletTable::const_iterator it = table->begin(u); !it.is_over(); ++it)
        {
            assert(it.treelet().get_structure()!=Treelet::invalid_structure);
            numTreelets2[it.treelet().get_structure()] += it.count(); //FIXME: Replace std::map ?
        }
    }


	for (const auto &[structure, count] : numTreelets2) // accumulate each treelet's count to its representant's count
	{
	    auto it = structure_to_representant.find(structure);
	    if(it!=structure_to_representant.end())
            numTreelets[it->second] += count;
	    else
        {
	        Treelet::treelet_structure_t repr = Treelet(structure).canonical_rooting().get_structure();
            structure_to_representant[structure] = repr;
            numTreelets[repr] += count;

            representant_to_structures[repr].push_back(structure);
        }

        totTreelets += count;
    }

	// init the residuals and the priorities -- the most frequent treelet comes first
	for (const auto &[structure, count] : numTreelets)
        treeletPriority.insert(structure, 100.0 + static_cast<double>(count) / static_cast<double>(totTreelets) );

	update_sampler();
}

/**
 * Pick the most efficient treelet(s) and rebuild the underlying sampler.
 */
void AdaptiveSampler::update_sampler()
{
	totTreeletSwitches++;
	current_treelet_structure = treeletPriority.last_key();

	delete treeletSelector;
	treeletSelector = new TreeletStructureSelector(TreeletStructureSelector::MODE_INCLUDE, representant_to_structures[current_treelet_structure].begin(), representant_to_structures[current_treelet_structure].end());

	delete sampler;
	sampler = new OccurrenceSampler(graph, ttc, size, false, true, true, buffer_size, buffer_degree);
	sampler->set_selector(treeletSelector, number_of_threads);
}

void AdaptiveSampler::recomputeTreeletPriorities(occ_info_table_t& occTab)
{
	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();

	// 1. subtract the frequency of completed graphlets
	std::map<Treelet::treelet_structure_t, double> treeletInefficiency; // estimated probability of yielding a graphlet in completedGraphlets
	for (const Occurrence &j : completedGraphlets)
	{
		const auto& occ_info = occTab[j];
		for(const auto &[spanning_repr, noccurences]: occurrences_to_spanning_representants[j])
		{
			const auto num_it = numTreelets.find(spanning_repr);
			if (num_it != numTreelets.cend())
				treeletInefficiency[spanning_repr] += ( static_cast<double>(noccurences) * static_cast<double>(occ_info.num_occurrences)) / (occ_info.weight * static_cast<double>(num_it->second));
		}
	}
	effTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();


	// 2. recompute inefficiencies for the affected treelets
	std::chrono::time_point < std::chrono::steady_clock > tstart1 = std::chrono::steady_clock::now();
	for(const auto &[treelet, ineff] : treeletInefficiency)
	{
		double prio = ((ineff<=1)?std::round(100 * (1.0 - ineff) ):0) + static_cast<double>(numTreelets[treelet]) / static_cast<double>(totTreelets);
		treeletPriority.insert(treelet, prio);
	}
	prioTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart1)).count();

	// Do we need to choose a new treelet?
	std::chrono::time_point < std::chrono::steady_clock > tstart2 = std::chrono::steady_clock::now();
	if (current_treelet_structure != treeletPriority.last_key())
		update_sampler();

	updateTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart2)).count();

	totManagementTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart)).count();
}


/**
 * Multi-threaded adaptive sampling.
 */
SampleTable* AdaptiveSampler::sample(const uint64_t n_samples, Random* rng, const double time_budget, const TreeletStructureSelector* build_selector)
{
	if(std::isnan(time_budget) || time_budget<=0 || (n_samples == 0 && std::isinf(time_budget)) ) //Either nothing to do or infinite samples
		return new SampleTable();

	occ_info_table_t occTab;
	occTab.set_empty_key(Occurrence());


	std::chrono::time_point < std::chrono::steady_clock > totTimeStart = std::chrono::steady_clock::now();
	uint64_t samples_rem = n_samples;
	while ((samples_rem > 0 || n_samples == 0) && totTime < time_budget) // take suffSamples more samples, in parallel
	{
		// 1. SAMPLE
		uint64_t round_samples = std::max(number_of_threads * 50u, suffSamples);

		if (n_samples!=0 && samples_rem<round_samples)
			round_samples = samples_rem;

        //FIXME: Avoid double copy into sample table?
		std::chrono::time_point < std::chrono::steady_clock > tstart_sample = std::chrono::steady_clock::now();
		SampleTable *samples = sampler->sample(round_samples, number_of_threads, rng);
		sampleTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_sample)).count();

		assert(samples->get_num_samples() == round_samples);
		samples_rem -= samples->get_num_samples();

		// 2. MERGE COUNTS
		std::chrono::time_point < std::chrono::steady_clock > tstart_merge = std::chrono::steady_clock::now();
		samples->sort_by_footprint();
		samples->group_by_footprint();
		bool recomputeTreelet = false;
		for(const auto &entry : *samples )
		{
			uint64_t &prev_occurrences = occTab[entry.occurrence].num_occurrences;
			if(prev_occurrences<suffSamples && prev_occurrences + entry.sample_count >= suffSamples)
			{
				completedGraphlets.insert(entry.occurrence);
				recomputeTreelet = true;
			}
			prev_occurrences += entry.sample_count;
		}
		mergeTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_merge)).count();


		//FIXME: Make multithreaded
        for(const auto &entry : *samples )
        {
            if(occurrences_to_spanning_representants.count(entry.occurrence)!=0)
                continue; //We already know the representants for this occurrence

            ColorCodingSpanningTreeCounter stc(&entry.occurrence, false, build_selector);
            stc.count();

            for(uint8_t u = 0; u < size; u++)
            {
                for(const auto &[t, nocc] : stc.get_table(u))
                {
                    const Treelet::treelet_structure_t representant_structure = t.canonical_rooting().get_structure();

                    uint64_t &nrepresentants = occurrences_to_spanning_representants[entry.occurrence][representant_structure];
                    if (nrepresentants==0)
		                representant_to_containing_occurrences[representant_structure].push_back(entry.occurrence);
                    
                    nrepresentants += nocc;
                }
            }
        }
        delete samples;


		// 3. UPDATE WEIGHTS
		std::chrono::time_point < std::chrono::steady_clock > tstart_w = std::chrono::steady_clock::now();
		//for all occurrences occ that contain t. Let C be the number of occurrences of t in occ
		for(const auto &occ : representant_to_containing_occurrences[current_treelet_structure])
		{
            const uint64_t noccs = occurrences_to_spanning_representants[occ][current_treelet_structure];
			occTab[occ].weight += static_cast<double>(round_samples) * static_cast<double>(noccs) / static_cast<double>(numTreelets[current_treelet_structure]);
		}

		weightsTime += (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - tstart_w)).count();

		if (recomputeTreelet)
			recomputeTreeletPriorities(occTab);

		totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - totTimeStart)).count();
		if (totTime >= time_budget)
			break;
	}

	//FIXME: Remove?
	std::cerr << "Time spent in sampling treelets: " << sampleTime << std::endl;
    std::cerr << "Time spent in merging counts: " << mergeTime << std::endl;
    std::cerr << "Time spent in weights update: " << weightsTime << std::endl;
    std::cerr << "Time spent in sampler update: " << updateTime << std::endl;
    std::cerr << "Time spent in computing efficiencies: " << effTime << std::endl;
    std::cerr << "Time spent in updating priorities: " << prioTime << std::endl;
    std::cerr << "Total treelet switches: " << totTreeletSwitches << std::endl;

	auto table = new SampleTable();
	for (const auto &[occurrence, info] : occTab)
	{
		SampleTable::Entry e;
		e.occurrence = occurrence;
		//By default e.num_spanning_trees = 0;
		e.sample_count = info.num_occurrences;
		e.estimated_graph_occurrences = static_cast<double>(e.sample_count) * (store_only_on_0 ? size : 1) / (info.weight);
		e.type = 'A';
		table->add_entry(e);
	}

	std::cerr << "Total management time: " << totManagementTime << std::endl;
	std::cerr << "Norm-2 of the sample distribution: " << table->norm2() << std::endl;
	return table;
}

