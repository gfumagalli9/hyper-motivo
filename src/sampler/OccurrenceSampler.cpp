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
#include <cmath>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include <cstring> // memcpy

// [HYPER] TLS per dedup degli iperarchi in un sample
namespace {
    thread_local std::vector<uint32_t> tls_seen_edge_build;
    thread_local uint32_t              tls_seen_epoch = 1;
}

void OccurrenceSampler::build_weak_and_incidence(const UndirectedGraph::vertex_t* U,
                                                 unsigned k,
                                                 uint8_t* out_bits,
                                                 std::vector<std::vector<uint8_t>>& M)
{
    // [HYPER] controlli minimi
    if (H == nullptr) {
        throw std::logic_error("OccurrenceSampler::build_weak_and_incidence: Hypergraph H non settato (usa set_hypergraph).");
    }
    assert(k == size && k <= 16);
    // azzera i bit di Gaifman (15 bytes per k<=16)
    std::memset(out_bits, 0, GaifmanBits::binary_footprint_bytes);

    // TLS mark-array per deduplicare gli iperarchi visitati in questo sample
    const uint32_t Medges = H->number_of_hyperedges();
    if (tls_seen_edge_build.size() != Medges)
        tls_seen_edge_build.assign(Medges, 0);
    uint32_t mark = ++tls_seen_epoch;
    if (tls_seen_epoch == 0) { // wrap-around safety
        std::fill(tls_seen_edge_build.begin(), tls_seen_edge_build.end(), 0);
        tls_seen_epoch = 1;
        mark = ++tls_seen_epoch;
    }

    // Accumula maschere (k-bit) per le restrizioni e∩U; dedup dopo con sort+unique
    std::vector<uint32_t> masks;
    masks.reserve(32);

    // Visita ogni iperarco incidente a qualche u∈U **una sola volta**
    for (unsigned iu = 0; iu < k; ++iu) {
        const auto v  = U[iu];
        const uint32_t dv = H->vertex_degree(v);
        for (uint32_t t = 0; t < dv; ++t) {
            const uint32_t e = H->incident_hyperedge(v, t);
            if (tls_seen_edge_build[e] == mark) continue;
            tls_seen_edge_build[e] = mark;

            // calcola e∩U con check O(k) (k<=16)
            unsigned  local_idx[16];
            unsigned  m = 0;
            uint32_t  mask = 0;

            const uint32_t sz = H->hyperedge_size(e);
            for (uint32_t j = 0; j < sz; ++j) {
                const auto w = H->hyperedge_vertex(e, j);
                for (unsigned i = 0; i < k; ++i) {
                    if (w == U[i]) {
                        local_idx[m++] = i;
                        mask |= (1u << i);
                        break;
                    }
                }
            }

            if (m >= 2) {
                // Gaifman: tutte le coppie in e∩U
                for (unsigned a = 1; a < m; ++a)
                    for (unsigned b = 0; b < a; ++b)
                        set_edge_bit(out_bits, local_idx[a], local_idx[b]);
                // Incidenza: registra la colonna per e∩U (weak)
                masks.push_back(mask);
            }
        }
    }

    // Dedup + ordinamento deterministico delle colonne
    if (!masks.empty()) {
        std::sort(masks.begin(), masks.end());
        masks.erase(std::unique(masks.begin(), masks.end()), masks.end());
    }

    // Converte in matrice densa k x b
    const uint16_t b = static_cast<uint16_t>(masks.size());
    M.assign(k, std::vector<uint8_t>(b, 0));
    for (uint16_t j = 0; j < b; ++j) {
        uint32_t mask = masks[j];
        for (uint16_t i = 0; i < k; ++i)
            if (mask & (1u << i)) M[i][j] = 1;
    }
}

void OccurrenceSampler::sample_thread_hyper(unsigned int thread_no, std::vector<HyperOccurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync)
{
	auto &terminate_flag = sync.get_termination_flag(thread_no);

	while(true)
	{
		sequencer_t::sequence_batch_t batch = sequencer->next_batch();
		if (batch.from >= batch.to_exclusive)
			break;

		for (uint64_t i = batch.from; i<batch.to_exclusive; i++)
		{
			HyperOccurrence o;
			sample_one_hyper(&o, rng);
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
HyperSampleTable* OccurrenceSampler::sample_hyper(const uint64_t num_samples, unsigned int number_of_threads, Random *rng, double time_budget)
{
	if(std::isnan(time_budget) || time_budget<=0 || (num_samples == 0 && std::isinf(time_budget)) ) //Either nothing to do or infinite samples
		return new HyperSampleTable();

	if (num_samples != 0 && num_samples < 10 * number_of_threads)
		number_of_threads = static_cast<unsigned int>((num_samples + 9) / 10 ); //ceil(samples/10)

	//Init per-thread structures
	TimeoutThreadSync threadSync(number_of_threads);
	auto threads = new std::thread[number_of_threads];
	auto rngs = new Random *[number_of_threads];
    auto samples = new std::vector<HyperOccurrence>[number_of_threads]();

    for (unsigned int i = 0; i < number_of_threads; i++)
		rngs[i] = rng->derived_rng();

	//Launch threads
	sequencer_t sequencer(0, (num_samples!=0)?num_samples:sequencer_t::to_max, number_of_threads);
	for (unsigned int i = 0; i < number_of_threads; i++)
		threads[i] = std::thread([this, i, samples, &sequencer, rngs, &threadSync] {
			sample_thread_hyper(i, samples[i], &sequencer, rngs[i], threadSync);
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
    auto sample_table = new HyperSampleTable();
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
