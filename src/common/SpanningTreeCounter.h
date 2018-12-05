/*
 * SpanningTreeCounter.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_SAMPLER_SPANNINGTREECOUNTER_H_
#define SRC_SAMPLER_SPANNINGTREECOUNTER_H_

#include "../common/Occurrence.h"
#include "../common/treelets/TreeletSelector.h"
#include <google/dense_hash_map>
#include <mutex>
#include <cstdio>

/**
 * Counts the spanning trees of an occurrence via color-coding
 */
class SpanningTreeCounter {
private:
	typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceFootprintHash,
			Occurrence::OccurrenceFootprintEquality> occ_table_t;
	occ_table_t cache;
	std::mutex m_mutex;

public:
	SpanningTreeCounter() {
		cache.set_empty_key(Occurrence());
	}
	;

	SpanningTreeCounter(std::string filename) {
		cache.set_empty_key(Occurrence());
		read_from_file(filename);
	}
	;

	/**
	 * Static methods: on-the-fly computation
	 */
	static uint64_t num_spanning_trees(const Occurrence &occ);
	static uint64_t num_spanning_trees(const Occurrence &occ, const TreeletSelector *ts);
	static unsigned int num_spanning_stars(const Occurrence &occ);
	static uint64_t num_spanning_trees_nostars(const Occurrence &occ);

	/**
	 * Instance methods: use the cache
	 */
	uint64_t get_spanning_trees(const Occurrence &occ, const TreeletSelector *ts);

	uint64_t size() {
		return cache.size();
	}

	/**
	 * Save to file, in binary format.
	 * The record format is:
	 * <k, fingerprint, edges>
	 */
	void save_to_file(std::string filename) {
		std::FILE* fd = std::fopen(filename.c_str(), "wb");
		if (!fd) {
	        std::cerr << "could not fopen() file to store STC: " << std::strerror(errno) << '\n';
			return;
		}
		occ_table_t::iterator itr = cache.begin();
		for (; itr != cache.end(); ++itr) {
			Occurrence o = itr->first;
			unsigned int k = o.get_size();
			std::fwrite(&k, sizeof(k), 1, fd);
			std::fwrite(&o.binary_footprint_bytes, sizeof(o.binary_footprint_bytes), 1, fd);
			const char* bf = o.binary_footprint();
			std::fwrite(bf, sizeof(*bf), o.binary_footprint_bytes, fd);
			std::fwrite(&(itr->second), sizeof(itr->second), 1, fd);
		}
		std::fclose(fd);
		std::cerr << "STC: written " << cache.size() << " entries to file" << std::endl;
	}

	/**
	 * Read from file
	 * The record format is:
	 * <k, fingerprint, edges>
	 */
	void read_from_file(std::string filename) {
		std::FILE* fd = std::fopen(filename.c_str(), "rb");
		if (!fd) {
	        std::cerr << "could not fopen() file to read STC: " << std::strerror(errno) << '\n';
			return;
		}
		unsigned int binary_footprint_bytes;
		uint8_t edges[Occurrence::binary_footprint_bytes];
		char* bf = reinterpret_cast<char*>(edges);
		uint64_t sptrees;
		unsigned int k;
		while (!std::feof(fd)) {
			std::fread(&k, sizeof(k), 1, fd);
			std::fread(&binary_footprint_bytes, sizeof(binary_footprint_bytes), 1, fd);
			memset(&edges, 0, sizeof(*edges) * binary_footprint_bytes);
			std::fread(bf, sizeof(*bf), binary_footprint_bytes, fd);
			std::fread(&sptrees, sizeof(sptrees), 1, fd);
			Occurrence o(k, reinterpret_cast<uint8_t*>(edges));
			cache[o] = sptrees;
		}
		std::fclose(fd);
		std::cerr << "STC: read " << cache.size() << " entries from file" << std::endl;
	}
};

#endif /* SRC_SAMPLER_SPANNINGTREECOUNTER_H_ */
