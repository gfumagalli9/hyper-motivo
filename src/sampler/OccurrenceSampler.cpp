//
// Created by steven on 9/11/17.
//

#include <thread>
#include "OccurrenceSampler.h"
#include "SampleTable.h"

void OccurrenceSampler::sample_thread(unsigned int thread_no, occ_count_table_t *table, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync)
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
			(*table)[o]++;

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
	occ_count_table_t *count_tabs = new occ_count_table_t[number_of_threads];
	for (unsigned int i = 0; i < number_of_threads; i++)
	{
		rngs[i] = rng->derived_rng();
		count_tabs[i].set_empty_key(Occurrence());
	}

	//Launch threads
	sequencer_t sequencer(0, (num_samples!=0)?num_samples:sequencer_t::to_max, number_of_threads);
	for (unsigned int i = 0; i < number_of_threads; i++)
		threads[i] = std::thread([this, i, count_tabs, &sequencer, rngs, &threadSync] {
			sample_thread(i, &count_tabs[i], &sequencer, rngs[i], threadSync);
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


	// cumulate counts into the first table
	occ_count_table_t &count_tab = count_tabs[0];
	for (unsigned int i = 1; i < number_of_threads; i++)
	{
		for (const auto &[occ, count] : count_tabs[i])
			count_tab[occ] += count;

		count_tabs[i].clear();
	}

	auto sample_table = build_sample_table(count_tab);

	//Cleanup
	delete[] threads;
	delete[] count_tabs;
	for (unsigned int i = 0; i < number_of_threads; i++)
		delete rngs[i];
	delete[] rngs;

	return sample_table;
}

SampleTable *OccurrenceSampler::build_sample_table(const OccurrenceSampler::occ_count_table_t &count_tab) const
{
	auto table = new SampleTable();
    for (const auto &[occ, count] : count_tab)
	{
		SampleTable::Entry e;
		e.fingerprint = occ.text_footprint();
		e.sample_count = count;
		table->addEntry(e);
	}
	return table;
}

void OccurrenceSampler::set_selector(const TreeletStructureSelector *new_sample_selector, const unsigned int number_of_threads)
{
	sampler.set_selector(new_sample_selector, number_of_threads);
}

OccurrenceSampler::OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size, bool vertices, bool graphlets, bool canonicize) :
		graph(graph), ttc(ttc), size(size), vertices(vertices), graphlets(graphlets), canonicize(canonicize), sampler(graph, ttc, size)
{}
