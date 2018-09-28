//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include <queue>
#include "OccurrenceSampler.h"
#include "SampleTable.h"
#include "../common/SpanningTreeCounter.h"

void OccurrenceSampler::do_sample_mt(occ_count_table_t* table, sequencer_t *sequencer, Random *rng)
{
	while(true)
	{
		sequencer_t::sequence_batch_t batch = sequencer->next_batch();
		if (batch.from >= batch.to)
			break;
		for (uint64_t i = batch.from; i < batch.to; i++) {
			Occurrence o;
			sample_one(&o, rng);
			(*table)[o]++;
		}
	}
	delete rng;
}


SampleTable OccurrenceSampler::sample(const uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget)
{
	SampleTable table;
	OccurrenceCanonicizer canon(size);
    if (num_samples == 0 || number_of_threads == 0)
		return table;

    if (num_samples / 10 < number_of_threads)
        number_of_threads = num_samples / 10;

	std::chrono::time_point < std::chrono::steady_clock > totTimeStart = std::chrono::steady_clock::now();

    if (number_of_threads <= 1)
    {
    	auto sampled_occurrences = new Occurrence[num_samples];
        for (uint64_t  i=0; i<num_samples; i++)
            sample_one(&sampled_occurrences[i], rng);
    }
    else
    {
        uint64_t samples_rem = num_samples;
    	occ_count_table_t* count_tabs = new occ_count_table_t[number_of_threads];
    	occ_count_table_t count_tab;
    	count_tab.set_empty_key(Occurrence());
		for (int id = 0; id < number_of_threads; id++)
    		count_tabs[id].set_empty_key(Occurrence());
		while (samples_rem > 0) {
			const uint64_t round_samples = std::min((uint64_t) 100 * number_of_threads, samples_rem);
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
						std::thread(
								[this, ct, sequencer, r] {do_sample_mt(ct, sequencer, r);}));
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
//					canon.canonicize(&o);
//					std::cout << id << " " << o.text_footprint() << " " << it.second << std::endl;
					count_tab[o] += it.second;
				}
				count_tabs[id].clear();
			}
			double totTime = (static_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - totTimeStart)).count();
//			std::cout << "totTime = " << totTime << std::endl;
			if (totTime >= time_budget)
				break;
		}

		SpanningTreeCounter stc;
		for (auto &it : count_tab) {
			Occurrence o = it.first;
			SampleTable::Entry e;
			e.fingerprint = o.text_footprint();
			e.occ = o;
			e.sample_count = it.second;
			e.num_spanning_trees = stc.num_spanning_trees(o, this->sampler.get_selector());
			table.addEntry(e);
		}
/*
		for (unsigned int i = 0; i < nthreads; i++)
        {
            Random* r = rng->derived_rng();
            worker_threads[i] = std::thread( [this, sampled_occurrences, sequencer, r] { do_sample_mt(sampled_occurrences, sequencer, r); });
        }

        for (unsigned int i = 0; i < nthreads; i++)
            worker_threads[i].join();
*/
    }

	return table;

}


void OccurrenceSampler::set_selector(const TreeletSelector *selector, unsigned int number_of_threads)
{
    sampler.set_selector(selector, number_of_threads);
}
