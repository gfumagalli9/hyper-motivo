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

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include <limits>
#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "Occurrence.h"
#include "TreeletSampler.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"
#include "SpanningTreeCounter.h"
#include "TimeoutThreadSync.h"

class OccurrenceSampler
{
private:
    typedef DynamicSequencer<uint64_t> sequencer_t;

    const UndirectedGraph *graph;
	const TreeletTableCollection *ttc;
	const unsigned int size;

	const bool vertices;
	const bool graphlets;
	const bool canonicize;

	TreeletSampler sampler;


	void sample_thread [[gnu::hot, gnu::flatten]](unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync);

public:
	inline void sample_one [[gnu::hot]] (Occurrence *occurrence, Random *rng)
	{
		UndirectedGraph::vertex_t sampled_vertices[16] = {0};
		UndirectedGraph::vertex_t root = sampler.sample_root(rng);
		assert(root < graph->number_of_vertices());
		Treelet t = sampler.sample_treelet(root, rng);

		static thread_local OccurrenceCanonicizer canonicizer(size);

		while (true)
		{
			if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
			{
#ifndef NDEBUG
				bool success =
#endif
						sampler.sample_rooted_occurrence(t, root, sampled_vertices, rng); //FIXME: Handle case in which there are no treelets
				assert(success);
			}

			if (graphlets)
				new(occurrence) Occurrence(size, graph, sampled_vertices);
			else
				new(occurrence) Occurrence(t, sampled_vertices);

			if(canonicize)
				canonicizer.canonicize(occurrence);

			break;
		}
	}

	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

	OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size, bool vertices,
			bool graphlets, bool canonicize, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree);

    ///@param sample_selector contains all the structures that we are interested in sampling.
	void set_selector(const TreeletStructureSelector *new_sample_selector, unsigned int number_of_threads);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
