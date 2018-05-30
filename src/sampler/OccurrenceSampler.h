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

// Flips a pair<A,B> into pair<B,A>
template<typename A, typename B>
std::pair<B, A> flip_pair(const std::pair<A, B> &p) {
	return std::pair<B, A>(p.second, p.first);
}

// Returns the map indexed and sorted by value
template<typename A, typename B>
std::multimap<B, A> flip_map(const std::map<A, B> &src) {
	std::multimap<B, A> dst;
	std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), flip_pair<A, B>);
	return dst;
}

class OccurrenceSampler {
public:
	struct OccurrenceHash {
		bool check_footprints;
		bool check_vertices;
		OccurrenceHash(bool check_footprints, bool check_vertices) :
				check_footprints(check_footprints), check_vertices(check_vertices) {
		}
		;

		inline size_t operator()[[gnu::hot,gnu::flatten]] (const Occurrence &key) const
		{
			size_t seed;
			seed = key.is_valid()?0xcb7fedb03a45866f:0xb896186490f1c8e9;

			if(check_footprints)
			{
				const char* p = key.binary_footprint();
				for (unsigned int i = 0; i < Occurrence::binary_footprint_bytes; i++)
				seed ^= static_cast<unsigned char>(p[i])*0xff51afd7ed558ccd +0x9e3779b9 + (seed << 6) + (seed >> 2);
			}

			if(check_vertices)
			{
				const UndirectedGraph::vertex_t* verts = key.vertices();
				for (unsigned int i = 0; i < 16; i++)
				seed ^= verts[i]*0xff51afd7ed558ccd + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			}

			return seed;
		}
	};

	struct OccurrenceEquality
	{
		bool check_footprints;
		bool check_vertices;
		OccurrenceEquality(bool check_footprints, bool check_vertices) : check_footprints(check_footprints), check_vertices(check_vertices) {};

		inline bool operator() [[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
		{
			return (occ1.is_valid()==occ2.is_valid()) &&
			(!check_footprints || !memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes)) &&
			(!check_vertices|| !memcmp(occ1.vertices(), occ2.vertices(), sizeof(UndirectedGraph::vertex_t) * 16));
		}
	};

	typedef DynamicSequencer<uint64_t> sequencer_t;
	typedef google::dense_hash_map<Occurrence , uint64_t, OccurrenceHash, OccurrenceEquality> table_t;

	static constexpr size_t buffer_size=1024*1024; //1MiB

	static constexpr unsigned int count_digits_ub = 1 + std::numeric_limits<uint64_t>::digits/3;
	static constexpr unsigned int vertex_no_digits_ub = 1 + std::numeric_limits<uint64_t>::digits/3;
	static constexpr unsigned int spanning_tree_no_digits_ub = 1 + std::numeric_limits<UndirectedGraph::vertex_t>::digits/3;

	static constexpr unsigned int max_occurrence_size =
	count_digits_ub + 1// count
	+ Occurrence::text_footprint_bytes + 1//text footprint
	+ spanning_tree_no_digits_ub + 1//spanning trees
	+ 16 * (vertex_no_digits_ub + 1)//verices
	+ 1;//newline

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

	void do_sample_st [[gnu::hot, gnu::flatten]] (table_t* count_table, int num_samples);
	void do_sample_mt [[gnu::hot, gnu::flatten]] (sequencer_t *sequencer, ConcurrentWriter *writer, table_t* count_table, int num_samples);
	inline void sample_one [[gnu::hot]] (Occurrence *occurrence);

	table_t* create_table();
	void merge_tables(table_t **count_tables);
	void write_table(table_t *count_table);
	void write_table_2(table_t *count_table);

	char* write(Occurrence *occurrence, char* buf);

public:
	void sample(int n_samples);
	table_t* sample(int n_samples, int number_of_threads);

	OccurrenceSampler(UndirectedGraph *graph, TreeletTableCollection* ttc, unsigned int size, Random *rng,
			bool vertices, bool graphlets, bool spanning_trees_no, bool footprints, bool canonicize, bool no_rejection,
			bool text, bool group_same, std::ostream *out, unsigned int number_of_threads, TreeletSelector* selector = nullptr, uint128_t tot_treelets = 0, bool store_only_0 = false);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
