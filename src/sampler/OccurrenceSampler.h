//
// Created by steven on 9/11/17.
//

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include <google/dense_hash_map>
#include <map>

#include "../common/graph/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "Occurrence.h"
#include "../common/sequencer/DynamicSequencer.h"
#include "../common/io/ConcurrentWriter.h"

class OccurrenceSampler {
public:
	typedef DynamicSequencer<uint64_t> sequencer_t;
	typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceHash,
			Occurrence::compare_eq> table_t;

	static constexpr size_t buffer_size = 1024 * 1024; //1MiB

	static constexpr unsigned int count_digits_ub = 1 + std::numeric_limits<uint64_t>::digits / 3;
	static constexpr unsigned int vertex_no_digits_ub = 1
			+ std::numeric_limits<uint64_t>::digits / 3;
	static constexpr unsigned int spanning_tree_no_digits_ub = 1
			+ std::numeric_limits<UndirectedGraph::vertex_t>::digits / 3;

	static constexpr unsigned int max_occurrence_size = count_digits_ub + 1 // count
			+ Occurrence::text_footprint_bytes + 1 //text footprint
			+ spanning_tree_no_digits_ub + 1 //spanning trees
			+ 16 * (vertex_no_digits_ub + 1) //verices
			+ 1; //newline

private:
	UndirectedGraph *graph;
	TreeletTableCollection *ttc;
	const unsigned int size;
	const uint128_t tot_treelets;
	const bool store_only_0;
	Random *rng;

	const bool vertices;
	const bool graphlets;
	const bool spanning_trees_no;
	const bool footprints;
	const bool canonicize;
	const bool no_rejection;
	const bool text;
	const bool group_same;

	std::ostream *output;
	const unsigned int number_of_threads;

	TreeletSampler sampler;

	void do_sample_st[[gnu::hot, gnu::flatten]] (table_t* count_table, int num_samples);
	void do_sample_mt [[gnu::hot, gnu::flatten]] (sequencer_t *sequencer, ConcurrentWriter *writer, table_t* count_table, int num_samples);

	void merge_tables(table_t **count_tables);
	void write_table(table_t *count_table);
	void write_table_2(table_t *count_table);

	char* write(Occurrence *occurrence, char* buf);

public:
	static table_t* create_table(const bool, const bool);

	inline void sample_one [[gnu::hot]] (Occurrence *occurrence) {
		UndirectedGraph::vertex_t sampled_vertices[16];
		UndirectedGraph::vertex_t root = sampler.sample_root();
		assert(root < graph->number_of_vertices());
		Treelet t = sampler.sample_treelet(root);

		while (true) {
			if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
			{
#ifndef NDEBUG
				bool success =
#endif
				sampler.sample_rooted_occurrence(t, root, sampled_vertices); //FIXME: Handle case in which there are no treelets
				assert(success);
			}

			if (graphlets) {
				new (occurrence) Occurrence(size, graph, sampled_vertices);

				if (!no_rejection
						&& rng->random_uint<uint64_t>(0, occurrence->number_of_spanning_trees() - 1)
						!= 0)
				continue; //Rejection
			} else
			new (occurrence) Occurrence(t, sampled_vertices);

			break;
		}
	}

	void sample(int n_samples);
	table_t* sample(int n_samples, int number_of_threads);

	OccurrenceSampler(UndirectedGraph *graph, TreeletTableCollection* ttc, unsigned int size, Random *rng,
			bool vertices, bool graphlets, bool spanning_trees_no, bool footprints, bool canonicize, bool no_rejection,
			bool text, bool group_same, std::ostream *out, unsigned int number_of_threads, TreeletSelector* selector = nullptr, uint128_t tot_treelets = 0, bool store_only_0 = false);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
