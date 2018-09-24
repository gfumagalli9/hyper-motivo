//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include "OccurrenceSampler.h"

void OccurrenceSampler::do_sample_mt(Occurrence* sampled_occurrences, sequencer_t *sequencer, Random *rng)
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


Occurrence* OccurrenceSampler::sample(const uint64_t num_samples, unsigned int number_of_threads, Random *rng)
{
    if (num_samples == 0 || number_of_threads == 0)
        return nullptr;

    unsigned int nthreads = number_of_threads;
    if(nthreads < num_samples / 10)
        nthreads = static_cast<unsigned int>(num_samples / 10);

    auto sampled_occurrences = new Occurrence[num_samples];

    if (nthreads <= 1)
    {
        std::cout << "OccurrenceSampler::sample || sampling " << num_samples << " graphlet occurrences" << std::endl;
        OccurrenceCanonicizer canonicizer(size);

        for (uint64_t  i=0; i<num_samples; i++)
            sample_one(&sampled_occurrences[i], rng);
    }
    else
    {
        auto sequencer = new sequencer_t(0, num_samples-1, nthreads);
        auto worker_threads = new std::thread[nthreads];
        for (unsigned int i = 0; i < nthreads; i++)
        {
            Random* r = rng->derived_rng();
            worker_threads[i] = std::thread( [this, sampled_occurrences, sequencer, r] { do_sample_mt(sampled_occurrences, sequencer, r); });
        }

        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
        delete sequencer;
    }

    return sampled_occurrences;

}


void OccurrenceSampler::set_selector(const TreeletSelector *selector, unsigned int number_of_threads)
{
    sampler.set_selector(selector, number_of_threads);
    std::cerr<< "treelet selector set to " << sampler.get_selector() << std::endl;
}
