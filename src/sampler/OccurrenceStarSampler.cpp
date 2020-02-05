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

#include <thread>
#include <queue>
#include <algorithm>
#include "OccurrenceStarSampler.h"
#include "../common/util.h"
#include "../sampler/SampleTable.h"
#include "SpanningTreeCounter.h"
#include <random>

OccurrenceStarSampler::OccurrenceStarSampler(const UndirectedGraph* g, unsigned int size, bool canonicize) :
        g(g), size(size), canonicize(canonicize)
{
#ifdef MOTIVO_STAR_SAMPLER_FLOATS
    double* weights = new double[g->number_of_vertices()];
    for (UndirectedGraph::vertex_t v = 0; v < g->number_of_vertices(); v++) {
        weights[v] = binomial(g->degree(v), size - 1);
	tot_stars += weights[v];
    }
    root_sampler_dbl = new std::discrete_distribution<UndirectedGraph::vertex_t>(weights, weights+g->number_of_vertices());
#else
    root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>(g->number_of_vertices());

    for (UndirectedGraph::vertex_t v = 0; v < g->number_of_vertices(); v++)
        root_sampler->set(v, static_cast<uint128_t>(binomial(g->degree(v), size - 1)+0.5)); //FIXME: Check type size. Fix return type of binomial

    root_sampler->build();
#endif
}

OccurrenceStarSampler::~OccurrenceStarSampler()
{
#ifdef MOTIVO_STAR_SAMPLER_FLOATS
    if(root_sampler_dbl)
        delete root_sampler_dbl;
#else
    if(root_sampler)
        delete root_sampler;
#endif
}

/**
 * Draw one sample.
 */
void OccurrenceStarSampler::sample_one(Occurrence *occurrence, Random *rng)
{
    static thread_local OccurrenceCanonicizer canonicizer(size);

    //FIXME: Handle the case of no stars to sample
#ifdef MOTIVO_STAR_SAMPLER_FLOATS
    UndirectedGraph::vertex_t r = (*root_sampler_dbl)(*(rng->underlying_generator()));
#else
    UndirectedGraph::vertex_t r = root_sampler->sample(rng);
#endif

    const UndirectedGraph::vertex_t d = g->degree(r);
    assert(size - 1 <= d);

    // now we select (size-1) indices picked u.a.r. from {0,...,d-1}
    static thread_local UndirectedGraph::vertex_t buf[sampling_vs_shuffling_degree_threshold]; //FIXME: avoid big stack allocation?
    if (d <= sampling_vs_shuffling_degree_threshold)
    {
        // we use Knuth's shuffle
        for (UndirectedGraph::vertex_t i = 0; i < d; i++) // we will put our occurrence in buf[0],...,buf[size-1]
            buf[i] = i;

        for (unsigned int i = 0; i < size - 1; i++) //stop early (we don't need buf[size-1],...,buf[d-1])
        {
            UndirectedGraph::vertex_t j = rng->random_uint<UndirectedGraph::vertex_t>(i, d - 1);
            UndirectedGraph::vertex_t t = buf[j];
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
            // with probability > 81.47%  we'll get (size-1) distinct indices (<1.228 iterations needed in expectation)
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
    for (unsigned int i = 0; i < size - 1; i++)
        buf[i] = g->neighbor(r, buf[i]);

    buf[size - 1] = r;
    new (occurrence) Occurrence(size, g, buf);

    if (canonicize)
        canonicizer.canonicize(occurrence);

}

void OccurrenceStarSampler::sample_thread(unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync)
{
    auto &terminate_flag = sync.get_termination_flag(thread_no);

    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if (batch.from >= batch.to_exclusive)
            break;

        for (uint64_t i = batch.from; i<batch.to_exclusive; i++)
        {
            Occurrence o;
            sample_one(&o, rng);
            samples.push_back(o);

            if(terminate_flag) //FIXME: Do we want to check at every iteration?
                goto end;
        }
    }

    end:
    sync.signal_termination_one();
}



SampleTable* OccurrenceStarSampler::sample(const uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget)
{
    if(std::isnan(time_budget) || time_budget<=0 || (num_samples == 0 && std::isinf(time_budget)) ) //Either nothing to do or infinite samples
        return new SampleTable();

    if (num_samples != 0 && num_samples < 10 * number_of_threads)
        number_of_threads = static_cast<unsigned int>((num_samples + 9) / 10 ); //ceil(samples/10)

    //Init per-thread structures
    TimeoutThreadSync threadSync(number_of_threads);
    auto threads = new std::thread[number_of_threads];
    auto rngs = new Random *[number_of_threads];
    auto samples = new std::vector<Occurrence>[number_of_threads]();

    for (unsigned int i = 0; i < number_of_threads; i++)
        rngs[i] = rng->derived_rng();

    //Launch threads
    sequencer_t sequencer(0, (num_samples!=0)?num_samples:sequencer_t::to_max, number_of_threads);
    for (unsigned int i = 0; i < number_of_threads; i++)
        threads[i] = std::thread([this, i, samples, &sequencer, rngs, &threadSync] {
            sample_thread(i, samples[i], &sequencer, rngs[i], threadSync);
        });

    //Wait for the threads to be done or for time_budget seconds
    if (!std::isinf(time_budget))
        threadSync.wait_timeout(time_budget);
    else
        threadSync.wait();

    //Either all the threads are done already or we hit a timeout. Ask the threads to terminate regardless
    threadSync.request_termination();
    for (unsigned int i = 0; i < number_of_threads; i++)
        threads[i].join();


    // Create sample table
    auto sample_table = new SampleTable();
    for (unsigned int i = 0; i < number_of_threads; i++)
    {
        sample_table->add_occurrences(samples[i].begin(), samples[i].end(), 'S');
        samples[i].clear();
    }


    //Cleanup
    delete[] samples;
    delete[] threads;
    for (unsigned int i = 0; i < number_of_threads; i++)
        delete rngs[i];
    delete[] rngs;

    return sample_table;
}

