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
#include "OccurrenceSampler.h"
#include "SampleTable.h"

void OccurrenceSampler::sample_thread(unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync)
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

/***
 * Main entry method for sampling, both single- and multi-threaded.
 *
 */
SampleTable* OccurrenceSampler::sample(const uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget)
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
		sample_table->add_occurrences(samples[i].begin(), samples[i].end(), 'N');
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

void OccurrenceSampler::set_selector(const TreeletStructureSelector *new_sample_selector, const unsigned int number_of_threads)
{
	sampler.set_selector(new_sample_selector, number_of_threads);
}

OccurrenceSampler::OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size,
		bool vertices, bool graphlets, bool canonicize, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree) :
		graph(graph), ttc(ttc), size(size), vertices(vertices), graphlets(graphlets), canonicize(canonicize),
		sampler(graph, ttc, size, buffer_size, buffer_degree)
{}
