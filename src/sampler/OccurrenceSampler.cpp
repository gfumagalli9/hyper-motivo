//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include <queue>
#include "OccurrenceSampler.h"
#include "SampleTable.h"
#include "../common/SpanningTreeCounter.h"

void OccurrenceSampler::do_sample_mt(occ_count_table_t* table, sequencer_t *sequencer,
		Random *rng) {
	while (true) {
		sequencer_t::sequence_batch_t batch = sequencer->next_batch();
		if (batch.from >= batch.to)
			break;
		for (uint64_t i = batch.from; i < batch.to; i++) {
			Occurrence o;
			sample_one(&o, rng);
			(*table)[o]++;
			if ((*table)[o] <= 2)
				stc->get_spanning_trees(o, sp_counter_selector);
		}
	}
	delete rng;
}
/***
 * Main entry method for sampling, both single- and multi-threaded.
 *
 */
SampleTable* OccurrenceSampler::sample(const uint64_t num_samples, unsigned int number_of_threads,
		Random *rng, double time_budget) {
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
		/**
		 * Single-threaded sampling.
		 */
		Occurrence o;
		uint64_t i = 0;
		while (i < num_samples || (on_budget && totTime < time_budget)) {
			sample_one(&o, rng);
			count_tab[o]++;
			totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
					- totTimeStart)).count();
		}
	} else {
		/**
		 * Multi-threaded sampling.
		 * It is done in (small) batches; each thread saves its results into a count
		 * table, and tables are then summed.
		 */
		uint64_t batch_size = 10; // samples batch size (per thread)
		uint64_t samples_rem = num_samples;
		occ_count_table_t* count_tabs = new occ_count_table_t[number_of_threads];
		for (int id = 0; id < number_of_threads; id++)
			count_tabs[id].set_empty_key(Occurrence());
		while (samples_rem > 0 || (on_budget && totTime < time_budget)) {
//			std::cout << "elapsed " << totTime << "/" << time_budget << std::endl;
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
//				std::cout << "Thread " << id << " samples " << thread_samples << std::endl;
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
			totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()
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
	// produce the counts
//	SpanningTreeCounter stc;
//	std::cout << "STC cache size: " << stc->size() << std::endl;
//	std::cout << "fillin sample table" << std::endl;
//	if (sp_counter_selector)
//		std::cout << sp_counter_selector->get_size() << std::endl;
	for (auto &it : count_tab) {
		Occurrence o = it.first;
		SampleTable::Entry e;
		e.fingerprint = o.text_footprint();
		e.occ = o;
		e.sample_count = it.second;
		e.num_spanning_trees = stc->get_spanning_trees(o, sp_counter_selector);
		table->addEntry(e);
	}
//	std::cout << "STC cache size: " << stc->size() << std::endl;
	return table;
}

void OccurrenceSampler::set_selector(const TreeletSelector *selector,
		unsigned int number_of_threads, const TreeletSelector *sp) {
	if (sp)
		this->sp_counter_selector = new TreeletSelector(*sp);
	sampler.set_selector(selector, number_of_threads);
}
