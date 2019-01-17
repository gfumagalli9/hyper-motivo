/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include <string>
#include "SpanningTreeCounter.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/util.h"
#include "ColorCodingSpanningTreeCounter.h"
#include "CachedSTC.h"


/**
 * Compute the number of spanning trees of the occurrence
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ) {
	return occ.number_of_spanning_trees();
}

/**
 * Compute the number of spanning trees of the occurrence, excluding stars
 */
uint64_t SpanningTreeCounter::num_spanning_trees_nostars(const Occurrence& occ) {
	return occ.number_of_spanning_trees() - num_spanning_stars(occ);
}

/**
 * Compute the number of spanning trees of the occurrence, possibly including/excluding some
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ, const TreeletSelector* ts)
{
	if (ts == nullptr || ts->number_of_treelets() == 0)
		return occ.number_of_spanning_trees();
	ColorCodingSpanningTreeCounter ccstc(&occ, ts);
	ccstc.count();
	return ccstc.number_of_spanning_trees();
}

/**
 * Return the number of spanning stars
 */
unsigned int SpanningTreeCounter::num_spanning_stars(const Occurrence& occ)
{
	unsigned int count = 0;
	for(unsigned int u=0; u<occ.get_size(); u++)
	{
		unsigned int deg=0;
		for(unsigned int v=0; v<u; v++)
			deg+=occ.has_edge(u,v);

		for(unsigned int v=u+1; v<occ.get_size(); v++)
			deg+=occ.has_edge(v,u);

		count += (deg == occ.get_size());
	}

	return count;
}

/**
 * Return the number of spanning trees from the cache (else compute it now).
 */
uint64_t SpanningTreeCounter::get_spanning_trees(const Occurrence& occ, const TreeletSelector* ts)
{
	if (!cache.count(occ))
	{
			m_mutex.lock();
			cache[occ] = SpanningTreeCounter::num_spanning_trees(occ, ts);
			m_mutex.unlock();
	}

	return cache[occ];
}

void SpanningTreeCounter::save_to_file(std::string filename)
{
    std::FILE* fd = std::fopen(filename.c_str(), "wb");
    if (!fd)
    	throw std::runtime_error("Could not fopen() file to store Spanning Tree Counter");

    occ_table_t::iterator itr = cache.begin();
    for (; itr != cache.end(); ++itr) {
        Occurrence o = itr->first;
        unsigned int k = o.get_size();
        std::fwrite(&k, sizeof(k), 1, fd);
        std::fwrite(&Occurrence::binary_footprint_bytes, sizeof(Occurrence::binary_footprint_bytes), 1, fd);
        const char* bf = o.binary_footprint();
        std::fwrite(bf, sizeof(*bf), o.binary_footprint_bytes, fd);
        std::fwrite(&(itr->second), sizeof(itr->second), 1, fd);
    }
    std::fclose(fd);
}

void SpanningTreeCounter::read_from_file(std::string filename)
{
	std::FILE* fd = std::fopen(filename.c_str(), "rb");
	if (!fd)
		throw std::runtime_error("Could not fopen() file to read Spanning Tree Counter");

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
}
