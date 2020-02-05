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

#ifndef SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_
#define SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_

#include <random>
#include "../common/graph/UndirectedGraph.h"
#include "../common/random/AliasMethodSampler.h"
#include "Occurrence.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"
#include "TimeoutThreadSync.h"

class OccurrenceStarSampler
{
public:
    typedef DynamicSequencer<uint64_t> sequencer_t;

private:
    static constexpr UndirectedGraph::vertex_t sampling_vs_shuffling_degree_threshold = 1024;

	const UndirectedGraph *g; // the host graph
    const unsigned int size; // k, the size of the stars
	const bool canonicize; // whether to canonicalize the occurrences

#ifdef MOTIVO_STAR_SAMPLER_FLOATS
    std::discrete_distribution<UndirectedGraph::vertex_t>* root_sampler_dbl = nullptr;
    double tot_stars = 0;
#else
    AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>* root_sampler = nullptr;
#endif

	void sample_one(Occurrence* occurrence, Random* rng);

	void sample_thread(unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync);

public:
    OccurrenceStarSampler(const UndirectedGraph *g, unsigned int size, bool canonicize);

    ~OccurrenceStarSampler();

#ifdef MOTIVO_STAR_SAMPLER_FLOATS
    double number_of_stars() const { return  tot_stars; }
#else
    uint128_t number_of_stars() const { return root_sampler->get_total_weight(); }
#endif

    SampleTable* sample(uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget);
};

#endif /* SRC_SAMPLER_OCCURRENCESTARSAMPLER_H_ */
