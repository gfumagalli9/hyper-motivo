/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include "doctest.h"
#include <sstream>
#include <cmath>
#include "../src/common/graph/UndirectedGraph.h"
#include "../src/sampler/Occurrence.h"
#include "../src/common/SpanningTreeCounter.h"

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 */

void test_stc(unsigned int from, unsigned int size) {
	UndirectedGraph test_graph("test-graph");
	UndirectedGraph::vertex_t* subgraph = new UndirectedGraph::vertex_t[size];
	for (unsigned int i = 0; i < size; i++)
		subgraph[i] = from + i;
	Occurrence occ(size, &test_graph, subgraph);
	UndirectedGraph g1(occ);
/*
	for (UndirectedGraph::vertex_t u = 0; u < g1.number_of_vertices(); u++) {
		std::cout << u << ": ";
		for (unsigned int i = 0; i < g1.degree(u); i++) {
			std::cout << " " << g1.neighbor(u, i);
		}
		std::cout << std::endl;
	}
*/
	SpanningTreeCounter stc;
	CHECK_EQ(stc.num_spanning_trees(occ), occ.number_of_spanning_trees());
	uint64_t tc = stc.num_spanning_trees(occ, nullptr);
	CHECK_EQ(tc, occ.number_of_spanning_trees());
	std::cout << tc << std::endl;
////	CHECK_EQ(g1.number_of_vertices(), size);
	delete[] subgraph;
}

TEST_CASE("SpanningTreeCounter.clique")
{
	test_stc(0, 3);
	test_stc(0, 4);
	test_stc(0, 5);
	test_stc(0, 12);
}

TEST_CASE("SpanningTreeCounter.star")
{
	// stars
	test_stc(16, 3);
	test_stc(16, 4);
	test_stc(16, 5);
	test_stc(16, 14);
}
