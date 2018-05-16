/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include "SpanningTreeCounter.h"
#include "graph/FullGraphColoring.h"
#include "treelets/TreeletTable.h"
#include "treelets/TreeletTableCollection.h"
#include "../builder/SimpleTreeletTableBuilder.h"
#include "../builder/TreeletTableBuilder.h"
#include <string>

SpanningTreeCounter::SpanningTreeCounter() {
	// TODO Auto-generated constructor stub
}

SpanningTreeCounter::~SpanningTreeCounter() {
	// TODO Auto-generated destructor stub
}

uint64_t SpanningTreeCounter::num_spanning_trees(Occurrence* occ) {
	uint64_t c = 0;
	return occ->number_of_spanning_trees();
}

uint64_t SpanningTreeCounter::num_spanning_trees(Occurrence* occ, TreeletSelector* ts) {
	std::cout << "Starting" << std::endl;

	UndirectedGraph h(occ);
	FullGraphColoring *coloring = new FullGraphColoring();
	TreeletTableCollection ttc;
	TreeletTable** tables = nullptr;
	ts = nullptr;

	std::cout << "Starting" << std::endl;

	// the table for 1-graphlets
	const std::string filename = "spantreecount." + std::to_string(1) + ".cnt";
	std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
	if (out.bad())
		throw std::runtime_error("Could not open output file for writing");
//	char buf[16] { "a" };
//	out.write(buf, 16 * sizeof("a"));
//	out.close();
//	return 0;

	SimpleTreeletTableBuilder builder(&h, coloring, 1, &ttc, &out, false, ts);
	builder.build();
	out.close();
	return 0;

//	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
	builder.build();
	std::cout << "BUILT TABLE 1" << std::endl;
//	std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
	CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias>* readers = nullptr;
	readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias> [h.number_of_vertices() - 1];
	tables = new TreeletTable*[h.number_of_vertices() - 1];
	for (unsigned int i = 2; i <= h.number_of_vertices(); i++) {
		// build tables of size i from those of size 1,...,i-1
		readers[i - 2].open("spantreecount." + std::to_string(i - 1) + ".dtz");
		readers[i - 2].prefault(0, h.number_of_vertices() - 1);
		tables[i - 2] = new TreeletTable(&readers[i - 2]);
		ttc.add(tables[i - 2]);
		const std::string filename = "spantreecount." + std::to_string(i) + ".cnt";
		std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
		SimpleTreeletTableBuilder builder(&h, coloring, i, &ttc, &out, i == h.number_of_vertices(),
				ts);
//        std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();
		builder.build();
//        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
//        std::cerr << "Building time: " << delta_t.count() << " s\n";
		out.close();
//        std::cout << "Output written to " << filename << std::endl;
	}

}
