/*
 * OccurrenceStarSampler.cpp
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#include <thread>
#include <queue>
#include <algorithm>
#include "OccurrenceStarSampler.h"
#include "../common/util.h"
#include "../sampler/SampleTable.h"

OccurrenceStarSampler::OccurrenceStarSampler(const UndirectedGraph* g, unsigned int size, unsigned int number_of_threads, bool canonicize) :
		g(g), size(size), number_of_threads(number_of_threads), canonicize(canonicize) {
	root_sampler = new AliasMethodSampler<UndirectedGraph::vertex_t, uint128_t>(
			g->number_of_vertices());
	for (UndirectedGraph::vertex_t v = 0; v < g->number_of_vertices(); v++)
		root_sampler->set(v, binomial(g->degree(v), size - 1)); //FIXME: Check type size. Fix return type of binomial

	root_sampler->build();
}

OccurrenceStarSampler::~OccurrenceStarSampler() {
	delete root_sampler;
}

/**
 * Draw one sample.
 */
void OccurrenceStarSampler::sample_one(Occurrence *occurrence, Random *rng) {
	static thread_local OccurrenceCanonicizer canonicizer(size);

	//FIXME: Handle the case of no stars to sample
	UndirectedGraph::vertex_t r = root_sampler->sample(rng);

	const UndirectedGraph::vertex_t d = g->degree(r);
	assert(size - 1 <= d);

	// now we select (size-1) indices picked u.a.r. from {0,...,d-1}
	static thread_local UndirectedGraph::vertex_t buf[sampling_vs_shuffling_degree_threshold]; //FIXME: avoid big stack allocation?
	if (d < sampling_vs_shuffling_degree_threshold) {
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
	} else {
		// random sampling
		bool are_distinct = false;
		//FIXME: We don't need to reroll all the indices
		// (when a duplicate is found, just reroll the previous index. In this way we also exploit the sorted
		// order when checking whether the next element is also duplicated)
		while (!are_distinct) {
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
	for (unsigned int i = 0; i < size - 1; i++)
		buf[i] = g->neighbor(r, buf[i]);

	buf[size - 1] = r;
	new (occurrence) Occurrence(size, g, buf);

	if (canonicize)
		canonicizer.canonicize(occurrence);

}

void OccurrenceStarSampler::do_sample_mt(occ_count_table_t* tab, sequencer_t *sequencer,
		Random* rng) {
	while (true) {
		sequencer_t::sequence_batch_t batch = sequencer->next_batch();
		if (batch.from >= batch.to_exclusive)
			break;

		for (uint64_t i = batch.from; i < batch.to_exclusive; i++) {
			Occurrence o;
			sample_one(&o, rng);
			(*tab)[o]++;
		}
	}

	delete rng;
}

/**
 * Take a given number of samples using a given number of threads.
 */
SampleTable* OccurrenceStarSampler::sample(uint64_t num_samples, Random *rng, double time_budget) {
	SampleTable* table = new SampleTable();
	bool on_budget = (num_samples == 0 && time_budget > 0
			&& time_budget != std::numeric_limits<double>::infinity());
	if (num_samples == 0 && !on_budget)
		return table;
	if (!on_budget && num_samples < 10 * number_of_threads)
		number_of_threads = std::ceil(1.0 * num_samples / 10);
	std::chrono::time_point < std::chrono::steady_clock > totTimeStart =
			std::chrono::steady_clock::now();
	double totTime = 0;
	occ_count_table_t count_tab;
	count_tab.set_empty_key(Occurrence());
	OccurrenceCanonicizer canon(size);
	if (number_of_threads <= 1) {
		Occurrence o;
		uint64_t i = 0;
		while ((i < num_samples || num_samples == 0) && totTime < time_budget) {
			sample_one(&o, rng);
			count_tab[o]++;
			totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
					- totTimeStart)).count();
		}
	} else {
		std::cout << number_of_threads << " " << on_budget << " " << num_samples << std::endl;
		uint64_t samples_rem = num_samples;
		uint64_t batch_size = 10; // samples batch size (per thread)
		occ_count_table_t* count_tabs = new occ_count_table_t[number_of_threads];
		for (int id = 0; id < number_of_threads; id++)
			count_tabs[id].set_empty_key(Occurrence());
		while (samples_rem > 0 || (on_budget && totTime < time_budget)) {
			std::chrono::time_point < std::chrono::steady_clock > roundStart =
					std::chrono::steady_clock::now();
			if (num_samples == 0)
				samples_rem = (uint64_t) batch_size * number_of_threads;
			const uint64_t round_samples = std::min((uint64_t) batch_size * number_of_threads,
					samples_rem);
			auto sequencer = new sequencer_t(0, round_samples, number_of_threads);
			uint64_t thread_samples = std::ceil(1.0 * round_samples / number_of_threads);
			std::queue<std::thread> thread_q;
			uint64_t round_samples_rem = round_samples;
			int id = 0;
			while (id < number_of_threads && round_samples_rem > 0) {
				thread_samples = std::min(thread_samples, round_samples_rem);
				Random* r = rng->derived_rng();
				auto ct = &count_tabs[id];
				thread_q.push(
						std::thread([this, ct, sequencer, r] {do_sample_mt(ct, sequencer, r);}));
				round_samples_rem -= thread_samples;
				samples_rem -= thread_samples;
				id++;
			}
			while (!thread_q.empty()) { // join threads
				thread_q.front().join();
				thread_q.pop();
			}
			delete sequencer;
			for (int id = 0; id < number_of_threads; id++) { // cumulate counts
				for (auto &it : count_tabs[id]) {
					Occurrence o = it.first;
					count_tab[o] += it.second;
				}
				count_tabs[id].clear();
			}
			double totTime =
					(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
							- totTimeStart)).count();
			if (totTime >= time_budget)
				break;
			double roundElapsed =
					(static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
							- roundStart)).count();
			// Adapt the batch size so to make the time per round approx 5% of the budget
			if (on_budget
					&& ((roundElapsed < 0.05 * time_budget) || (roundElapsed > 0.1 * time_budget)))
				batch_size *= (0.05 * time_budget / roundElapsed);
			batch_size = std::max(batch_size, 10ul);
		}
		delete[] count_tabs;
	}

	for (auto &it : count_tab)
	{
		Occurrence o = it.first;
		SampleTable::Entry e;
		e.fingerprint = o.text_footprint();
		e.occurrence = o;
		e.sample_count = it.second;
		e.num_spanning_trees = 0; //stc.num_spanning_stars(o); //FIXME!
		table->addEntry(e);
	}
	table->estimateFrequencies();
	return table;
}

