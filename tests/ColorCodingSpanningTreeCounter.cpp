#include "../src/sampler/Occurrence.h"

#include "doctest.h"
#include "../src/sampler/ColorCodingSpanningTreeCounter.h"

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 */

void test_ccstc(unsigned int from, unsigned int size)
{
	UndirectedGraph test_graph("test-graph");
	auto* subgraph = new UndirectedGraph::vertex_t[size];

	for (unsigned int i = 0; i < size; i++)
		subgraph[i] = from + i;

	Occurrence occ(size, &test_graph, subgraph);
    ColorCodingSpanningTreeCounter counter(&occ);
    counter.count();
	CHECK_EQ(counter.number_of_spanning_trees_rooted_at(0), occ.number_of_spanning_trees());

	delete[] subgraph;
}

TEST_CASE("ColorCodingSpanningTreeCounter.clique")
{
    for(unsigned int i=1; i<=8; i++)
        test_ccstc(0, i);
}

TEST_CASE("ColorCodingSpanningTreeCounter.star")
{
   for(unsigned int i=1; i<=12; i++)
        test_ccstc(16, i);
}
