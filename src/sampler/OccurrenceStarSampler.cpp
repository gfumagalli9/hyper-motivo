/*
 * OccurrenceStarSampler.cpp
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#include <thread>
#include <algorithm>
#include "OccurrenceStarSampler.h"
#include "../common/common.h"


OccurrenceStarSampler::OccurrenceStarSampler(const UndirectedGraph* g, unsigned int size, unsigned int number_of_threads, bool canonicize) :
        g(g), size(size), number_of_threads(number_of_threads), canonicize(canonicize)
{
    root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>(g->number_of_vertices());
    for (UndirectedGraph::vertex_t v = 0; v < g->number_of_vertices(); v++)
        root_sampler->set(v, binomial(g->degree(v), size - 1)); //FIXME: Check type size. Fix return type of binomial

    root_sampler->build();
}


OccurrenceStarSampler::~OccurrenceStarSampler()
{
    delete root_sampler;
}

/**
 * Draw one sample.
 */
void OccurrenceStarSampler::sample_one(Occurrence *occurrence, Random *rng)
{
	static thread_local OccurrenceCanonicizer canonicizer(size);

	//FIXME: Handle the case of no stars to sample
	UndirectedGraph::vertex_t r = root_sampler->sample(rng);

	const UndirectedGraph::vertex_t d = g->degree(r);
	assert(size - 1 <= d);
	
	// now we select (size-1) indices picked u.a.r. from {0,...,d-1}
	static thread_local UndirectedGraph::vertex_t buf[sampling_vs_shuffling_degree_threshold]; //FIXME: avoid big stack allocation?
	if (d < sampling_vs_shuffling_degree_threshold)
    {
        // we use Knuth's shuffle
		for (UndirectedGraph::vertex_t i = 0; i < d; i++) // we will put our occurrence in buf[0],...,buf[size-1]
			buf[i] = i;

		for (unsigned int i = 0; i < size - 1; i++) //stop early (we don't need buf[size-1],...,buf[d-1])
		{
            UndirectedGraph::vertex_t j = rng->random_uint<UndirectedGraph::vertex_t>(i, d - 1);
            UndirectedGraph::vertex_t t= buf[j];
			buf[j] = buf[i];
			buf[i] = t;
		}
	}
	else
    {
        // random sampling
		bool are_distinct = false;
        //FIXME: We don't need to reroll all the indices
        // (when a duplicate is found, just reroll the previous index. In this way we also exploit the sorted
        // order when checking whether the next element is also duplicated)
        while (!are_distinct)
		{
		    // with high probability we'll get (size-1) distinct indices quickly
			for (unsigned int i = 0; i < size - 1; i++)
				buf[i] = rng->random_uint<UndirectedGraph::vertex_t>(0, d - 1);

			std::sort(buf, buf + size - 1);
			are_distinct = true;
			for (unsigned int i = 1; i < size - 1 && are_distinct; i++)
				are_distinct = (buf[i] != buf[i - 1]);
		}
	}

	// our indices are in buf[0],...,buf[size - 2]
	// we convert the indices into actual nodes, and add the root
	for(unsigned int i = 0; i < size - 1; i++)
	    buf[i] = g->neighbor(r, buf[i]);

	buf[size - 1] = r;
	new (occurrence) Occurrence(size, g, buf);

	if (canonicize)
		canonicizer.canonicize(occurrence);

}



void OccurrenceStarSampler::do_sample_mt(Occurrence* sampled_occurrences, sequencer_t *sequencer, Random* rng)
{
    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if (batch.from >= batch.to)
            break;

        for (uint64_t i = batch.from; i < batch.to; i++)
            sample_one(&sampled_occurrences[i], rng);
    }

    delete rng;
}



/**
 * Take a given number of samples using a given number of threads.
 */
Occurrence* OccurrenceStarSampler::sample(uint64_t num_samples, Random *rng)
{
	if (num_samples <= 0 || number_of_threads <= 0)
		return nullptr;

    unsigned int nthreads = number_of_threads;
    if (num_samples / nthreads < 10)
        nthreads = static_cast<unsigned int>(num_samples / 10);

    auto sampled_occurrences = new Occurrence[num_samples];

    if(nthreads <= 1)
	{
		for (uint64_t i = 0; i < num_samples; i++)
			sample_one(&sampled_occurrences[i], rng);
	}
    else
    {
		auto worker_threads = new std::thread[nthreads];
        auto sequencer = new sequencer_t(0, num_samples-1, nthreads);

        for(unsigned int i=0; i<nthreads; i++)
		{
			Random* r = rng->derived_rng();
			worker_threads[i] = std::thread([this, sampled_occurrences, sequencer, r] { do_sample_mt(sampled_occurrences, sequencer, r); });
		}

		for (unsigned int i=0; i<nthreads; i++) {
			worker_threads[i].join();
		}

        delete[] worker_threads;
        delete sequencer;
    }

	return sampled_occurrences;
}

