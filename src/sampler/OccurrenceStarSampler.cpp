/*
 * OccurrenceStarSampler.cpp
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#include <thread>
#include <vector>
#include "OccurrenceStarSampler.h"
#include "../common/common.h"

OccurrenceStarSampler::~OccurrenceStarSampler() {
	delete root_sampler;
}

OccurrenceStarSampler::OccurrenceStarSampler(UndirectedGraph* g, unsigned int size,
		uint64_t num_samples, Random* rng, bool canonicize, bool no_rejection, bool group_same) {
	this->g = g;
	this->size = size;
	this->num_samples = num_samples;
	this->rng = rng;
	this->canonicize = canonicize;
	this->no_rejection = no_rejection;
	this->group_same = group_same;
	this->root_sampler = new DiscreteDistribution();
	//new RangeSampler<UndirectedGraph::vertex_t>(false);
	for (UndirectedGraph::vertex_t v = 0; v < g->number_of_vertices(); v++) {
		uint64_t b = binomial(g->degree(v), size - 1);
		this->root_sampler->add_bin(b);
	}
}

/**
 * Draw one sample.
 */
void OccurrenceStarSampler::sample_one(Occurrence *occurrence, UndirectedGraph::vertex_t r) {
	UndirectedGraph::vertex_t sampled_vertices[16];
//	std::cout << "sampling root vertex... " << std::endl;
	if (r == -1)
		r = this->root_sampler->sample(rng);
//	std::cout << "sampled root vertex " << r << std::endl;
	const UndirectedGraph::vertex_t d = g->degree(r);
//	std::cout << size - 1 << " " << d << std::endl;
	if (size - 1 > d)
		std::cout << binomial(d, size - 1) << std::endl;
	assert(size - 1 <= d);
	// now we select (size-1) indices picked u.a.r. from {0,...,d-1}
	const UndirectedGraph::vertex_t D = 1024;
	UndirectedGraph::vertex_t buf[D]; // we will put our occurrence in buf[0],...,buf[size-1]
	if (d < D) { // we use Knuth's shuffle
		for (UndirectedGraph::vertex_t v = 0; v < D; v++)
			buf[v] = v;
		for (unsigned int i = 0; i < size - 1; i++) {
			unsigned int j = rng->random_uint<UndirectedGraph::vertex_t>(0, d - 1 - i);
			unsigned int x = buf[i + j];
			buf[i + j] = buf[i];
			buf[i] = x;
		}
	} else { // random sampling
		bool are_distinct = false;
		while (!are_distinct) { // with high prob we'll get (size-1) distinct indices soon
			for (unsigned int i = 0; i < size - 1; i++)
				buf[i] = rng->random_uint<UndirectedGraph::vertex_t>(0, d - 1);
			std::sort(buf, buf + size - 1);
			are_distinct = true;
			for (unsigned int i = 1; i < size - 1; i++)
				are_distinct &= buf[i] != buf[i - 1];
		}
	}
	// our indices are in buf[0],...,buf[size - 2]
	// we convert the indices into actual nodes, and add the the root
	for (unsigned int i = 0; i < size - 1; i++)
		buf[i] = g->neighbor(r, buf[i]);
	buf[size - 1] = r;
	new (occurrence) Occurrence(size, g, buf);
}

/**
 * Single-thread sampling.
 */
void OccurrenceStarSampler::sample_many(OccurrenceSampler::table_t* count_table, int num_samples) {
	Occurrence occurrence;
	OccurrenceCanonicizer canonicizer(size);
	uint64_t* roots = new uint64_t[num_samples];
	this->root_sampler->sample(rng, roots, num_samples);
	for (uint64_t i = 0; i < num_samples; i++) {
		sample_one(&occurrence, roots[i]);
		if (canonicize)
			canonicizer.canonicize(&occurrence);
		(*count_table)[occurrence] += 1;
	}
	delete[] roots;
}

/**
 * Create an empty occurrence count table.
 */
OccurrenceSampler::table_t *OccurrenceStarSampler::create_table() {
	static Occurrence empty_key = Occurrence();
	static OccurrenceSampler::OccurrenceHash hasher = OccurrenceSampler::OccurrenceHash(true,
			false);
	static OccurrenceSampler::OccurrenceEquality eq = OccurrenceSampler::OccurrenceEquality(true,
			false);
	OccurrenceSampler::table_t* table = new OccurrenceSampler::table_t(0, hasher, eq);
	table->set_empty_key(empty_key);
	return table;
}

/**
 * Take a given number of samples using a given number of threads.
 */
OccurrenceSampler::table_t* OccurrenceStarSampler::sample(int num_samples, int number_of_threads) {
	if (num_samples <= 0 || number_of_threads <= 0)
		return nullptr;
	number_of_threads = std::min((num_samples + 9) / 10, number_of_threads);
	if (number_of_threads == 1) {
		OccurrenceSampler::table_t* count_table = create_table();
		sample_many(count_table, num_samples);
		return count_table;
	} else {
		OccurrenceSampler::table_t** count_tables = nullptr;
		count_tables = new OccurrenceSampler::table_t*[number_of_threads];
		std::vector<std::thread> worker_threads;
		int rem_samples = num_samples;
		int id = 0;
		while (rem_samples > 0) {
			int nsamples = std::min((num_samples + number_of_threads - 1) / number_of_threads,
					rem_samples);
			OccurrenceSampler::table_t* count_table = nullptr;
			count_table = count_tables[id++] = create_table();
			worker_threads.push_back(
					std::thread(
							[this, count_table, nsamples] {sample_many(count_table, nsamples);}));
			rem_samples -= nsamples;
		}
		for (std::thread& t : worker_threads)
			t.join();
		// Merge the tables
		for (unsigned int i = 1; i < worker_threads.size(); i++) {
			OccurrenceSampler::table_t::const_iterator it = count_tables[i]->begin();
			while (it != count_tables[i]->end()) {
				(*count_tables[0])[it->first] += it->second;
				it++;
			}
			count_tables[i]->clear();
		}
		OccurrenceSampler::table_t* t0 = count_tables[0];
		for (unsigned int i = 1; i < worker_threads.size(); i++)
			delete count_tables[i];
		delete[] count_tables;
		// Returned the merged table
		return t0;
	}
}

