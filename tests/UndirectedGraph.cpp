/*
 * UndirectedGraph.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include "doctest.h"
#include <sstream>
#include <cmath>
#include "../src/common/graph/UndirectedGraph.h"
#include "../src/sampler/Occurrence.h"

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 */

void test_ug(unsigned int from, unsigned int size) {
	UndirectedGraph test_graph("test-graph");
	UndirectedGraph::vertex_t* subgraph = new UndirectedGraph::vertex_t[size];
	for (unsigned int i = 0; i < size; i++)
		subgraph[i] = from + i;
	Occurrence occ(size, &test_graph, subgraph);
	UndirectedGraph g1(&occ);
    CHECK_EQ(g1.number_of_vertices(), size);
	delete[] subgraph;
}

TEST_CASE("The clique")
{
	test_ug(0, 16);
}

