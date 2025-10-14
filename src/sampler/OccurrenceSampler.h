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

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include <limits>
#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "Occurrence.h"
#include "TreeletSampler.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"
#include "SpanningTreeCounter.h"
#include "TimeoutThreadSync.h"

// DA ELIMINARE
#include "../hyper_sampler/HyperOccurrence.h"
#include "../hyper_sampler/HyperSampleTable.h"

class OccurrenceSampler
{
private:
    typedef DynamicSequencer<uint64_t> sequencer_t;

    const UndirectedGraph *graph;
	const TreeletTableCollection *ttc;
	const unsigned int size;

	const bool vertices;
	const bool graphlets;
	const bool canonicize;

	TreeletSampler sampler;

    // [HYPER] ipergrafo sorgente (opzionale): se settato abilita le utility hyper
    const Hypergraph* H = nullptr;

	void sample_thread [[gnu::hot, gnu::flatten]](unsigned int thread_no, std::vector<Occurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync);

	void sample_thread_hyper [[gnu::hot, gnu::flatten]](unsigned int thread_no, std::vector<HyperOccurrence>& samples, sequencer_t *sequencer, Random *rng, TimeoutThreadSync &sync);


	// [HYPER] layout identico a Occurrence::add_edge (upper-triangular, row-major)
	static inline void set_edge_bit(uint8_t* bits, unsigned i, unsigned j) {
		if (i == j) return;
		if (i < j) std::swap(i, j);
		const unsigned pos = (i - 1) * i / 2 + j;
		bits[pos / 8] |= static_cast<uint8_t>(0x80u >> (pos % 8));
	}
public:
	inline void sample_one_hyper [[gnu::hot]] (HyperOccurrence *occurrence, Random *rng){
		UndirectedGraph::vertex_t sampled_vertices[16] = {0};

		// estrai root e treelet come nel naive standard
		UndirectedGraph::vertex_t root = sampler.sample_root(rng);
		assert(root < graph->number_of_vertices());
		Treelet t = sampler.sample_treelet(root, rng);

		// *** QUI RIEMPIAMO U ***: campiona l'occorrenza radicata (k vertici)
		bool success = false;
		do {
			success = sampler.sample_rooted_occurrence(t, root, sampled_vertices, rng);
			// Se non esistessero occorrenze per (t,root), riprova
		} while (!success);
		// Ora sampled_vertices[0..size-1] è valorizzato

		// 1) Gaifman bits (15 bytes) su U
		uint8_t bits[GaifmanBits::binary_footprint_bytes] = {0};
		GaifmanBits gbits;

		if (graphlets) {
			// 2) Passata unica: Gaifman + incidenza weak e∩U (|e∩U|>=2)
			std::vector<std::vector<uint8_t>> M; // k x b (0/1)
			build_weak_and_incidence(sampled_vertices, size, bits, M);
			std::memcpy(gbits.bytes, bits, GaifmanBits::binary_footprint_bytes);

			// 3) Costruisci l'HyperOccurrence con (k, U, gbits, M)
			new (occurrence) HyperOccurrence(size, sampled_vertices, gbits, M,
											/*canonicalize_bipartite=*/canonicize);
		} else {
			// Degenerate: colonne = archi della treelet (child,parent)
			unsigned int parents[16] = {0};
			unsigned int current = 0, n = 0;
			for (Treelet::treelet_structure_t s = t.get_structure(); s; s <<= 1) {
				if (s & Treelet::treelet_structure_highest_bit) {
					++n;
					set_edge_bit(bits, n, current);
					parents[n] = current;
					current = n;
				} else {
					current = parents[current];
				}
			}
			assert(n == size - 1);

			std::memcpy(gbits.bytes, bits, GaifmanBits::binary_footprint_bytes);

			std::vector<std::vector<uint8_t>> M;
			HyperOccurrence::build_treelet_incidence_block(t, size, M);
			new (occurrence) HyperOccurrence(size, sampled_vertices, gbits, M,
											/*canonicalize_bipartite=*/canonicize);
		}
	}

	inline void sample_one [[gnu::hot]] (Occurrence *occurrence, Random *rng)
	{
		UndirectedGraph::vertex_t sampled_vertices[16] = {0};
		UndirectedGraph::vertex_t root = sampler.sample_root(rng);
		assert(root < graph->number_of_vertices());
		Treelet t = sampler.sample_treelet(root, rng);

		static thread_local OccurrenceCanonicizer canonicizer(size);

		while (true)
		{
			if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
			{
#ifndef NDEBUG
				bool success =
#endif
						sampler.sample_rooted_occurrence(t, root, sampled_vertices, rng); //FIXME: Handle case in which there are no treelets
				assert(success);
			}

			if (graphlets)
				new(occurrence) Occurrence(size, graph, sampled_vertices);
			else
				new(occurrence) Occurrence(t, sampled_vertices);

			if(canonicize)
				canonicizer.canonicize(occurrence);
			break;
		}
	}

	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

	HyperSampleTable* sample_hyper(uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

	OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size, bool vertices,
			bool graphlets, bool canonicize, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree);

    ///@param sample_selector contains all the structures that we are interested in sampling.
	void set_selector(const TreeletStructureSelector *new_sample_selector, unsigned int number_of_threads);

    // [HYPER] setter non-invasivo: passami l'ipergrafo quando vuoi usare le utility
    inline void set_hypergraph(const Hypergraph* H_ptr) { H = H_ptr; }

    // [HYPER] Utility: in **un'unica passata** su H costruisce
    //  - i bit di Gaifman del sottoipergrafo debolmente indotto su U (|e∩U|>=2)
    //  - la matrice di incidenza bipartita VxE (densa, k x b, colonne deduplicate)
    //
    // Precondizioni:
    //  - H != nullptr
    //  - k == size e k <= 16 (come nel naive)
    void build_weak_and_incidence(const UndirectedGraph::vertex_t* U,
                                  unsigned k,
                                  uint8_t* out_bits,
                                  std::vector<std::vector<uint8_t>>& M);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
