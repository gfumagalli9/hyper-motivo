//
// Created by steven on 9/11/17.
//

#include <thread>
#include "OccurrenceSampler.h"
#include "SampleTable.h"

void OccurrenceSampler::sample_thread(occ_count_table_t *table, sequencer_t *sequencer, Random *rng, std::atomic<bool> &terminate_flag, ThreadSync &sync)
{
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
	sync.terminate();
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
	static_assert(std::atomic<bool>::is_always_lock_free, "std::atomic<bool> is not always lock-free");

	auto threads = new std::thread[number_of_threads];
	auto terminate_flags = new std::atomic<bool>[number_of_threads];
	auto rngs = new Random *[number_of_threads];
	occ_count_table_t *count_tabs = new occ_count_table_t[number_of_threads];
	for (unsigned int i = 0; i < number_of_threads; i++)
	{
		terminate_flags[i] = false;
		rngs[i] = rng->derived_rng();
		count_tabs[i].set_empty_key(Occurrence());
	}

	ThreadSync sync(number_of_threads);


	//Launch threads
	sequencer_t sequencer(1, (num_samples!=0)?num_samples:sequencer_t::to_max, number_of_threads);
	for (unsigned int i = 0; i < number_of_threads; i++)
		threads[i] = std::thread([this, i, count_tabs, &sequencer, rngs, terminate_flags, &sync] {
			sample_thread(&count_tabs[i], &sequencer, rngs[i], terminate_flags[i], sync);
		});


	//Wait for the threads to be done or for time_budget seconds
	if (!std::isinf(time_budget))
		sync.wait_timeout(time_budget);
	else
		sync.wait();

	//Either all the threads are done already or we hit a timeout. Ask the threads to terminate regardless
	for (unsigned int i = 0; i < number_of_threads; i++)
		terminate_flags[i] = true;

	//Wait for threads to terminate (if not already)
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
	delete[] terminate_flags;
	for (unsigned int i = 0; i < number_of_threads; i++)
		delete rngs[i];
	delete[] rngs;

	return sample_table;
}

SampleTable *OccurrenceSampler::build_sample_table(const OccurrenceSampler::occ_count_table_t &count_tab) const
{
	auto table = new SampleTable();
	for (const auto &it : count_tab)
	{
		Occurrence o = it.first;
		SampleTable::Entry e;
		e.fingerprint = o.text_footprint();
		e.occurrence = o;
		e.sample_count = it.second;
		e.num_spanning_trees = 0; //stc.get_spanning_trees(o, sp_counter_selector); //FIXME: ?? Handle this. Make multi-threaded?
		table->addEntry(e);
	}
	return table;
}

void OccurrenceSampler::set_selector(const TreeletStructureSelector *new_sample_selector, const TreeletStructureSelector *new_build_selector, const unsigned int number_of_threads)
{
 	delete sample_selector;
	sample_selector=nullptr;

	delete build_selector;
	build_selector=nullptr;

	if(new_sample_selector!=nullptr)
	{
		sample_selector = new TreeletStructureSelector(new_sample_selector->restrict_to_sizes(size,size));

		if(new_build_selector != nullptr)
			build_selector = new TreeletStructureSelector(sample_selector->buildable_closure().intersection(*new_build_selector));
		else
			build_selector = new TreeletStructureSelector(sample_selector->buildable_closure());
	}
	else if(new_build_selector!=nullptr)
		build_selector = new TreeletStructureSelector(*new_build_selector);

	sampler.set_selector(sample_selector, number_of_threads);


	delete spanning_tree_counter;
	if(!no_rejection)
		spanning_tree_counter = new SpanningTreeCounter(size, build_selector);
}

OccurrenceSampler::OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size,
				  bool vertices, bool graphlets, bool canonicize, bool no_rejection) :
		graph(graph), ttc(ttc), size(size), vertices(vertices), graphlets(graphlets), canonicize(canonicize),
		no_rejection(no_rejection), sampler(graph, ttc, size)
{
	spanning_tree_counter = new SpanningTreeCounter(size);
}

OccurrenceSampler::~OccurrenceSampler()
{
	delete sample_selector;
	delete build_selector;
	delete spanning_tree_counter;
}
