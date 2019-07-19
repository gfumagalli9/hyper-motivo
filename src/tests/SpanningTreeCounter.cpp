/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include "doctest.h"
#include <sstream>
#include <cmath>
#include "../common/graph/UndirectedGraph.h"
#include "../sampler/SpanningTreeCounter.h"
#include "../sampler/Occurrence.h"
#include "../common/util.h"

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 * Four isolated vertices 56-59
 */

void test(unsigned int from, unsigned int size, uint64_t expected)
{
    UndirectedGraph test_graph("test-graph");

    auto subgraph = new UndirectedGraph::vertex_t[size];
    for(unsigned int i=0; i<size; i++)
        subgraph[i]=from+i;
    Occurrence occ(size, &test_graph,  subgraph);
    CHECK(SpanningTreeCounter::number_of_rooted_spanning_trees_kirchhoff(occ) == expected*size );
    CHECK(SpanningTreeCounter::number_of_rooted_spanning_trees_colorcoding(occ) == expected*size );

    delete[] subgraph;
}

TEST_CASE("SpanningTreeCounter misc")
{
    //Size 1 subgraph
    test(1, 1, 1);

    //A diamond
    test(48, 4, 8);

    //A paw
    test(52, 4, 3);
}

TEST_CASE("SpanningTreeCounter stars fast")
{
    for(unsigned int i=2; i<=13; i++)
        test(16, i, 1);
}

TEST_CASE("SpanningTreeCounter stars slow")
{
    for(unsigned int i=14; i<=16; i++)
        test(16, i, 1);
}

TEST_CASE("SpanningTreeCounter paths")
{
    //Look at the subgraph induced by the first i vertices of the path
    for(unsigned int i=2; i<=16; i++)
        test(32, i, 1);
}


TEST_CASE("SpanningTreeCounter cliques fast")
{
    //The number of spanning trees in K_n is num_elements**(num_elements-2) by Cayley's formula
    for(unsigned int i=2; i<=9; i++)
        test(0, i, ipow<uint64_t>(i, i-2));
}

TEST_CASE("SpanningTreeCounter cliques slow")
{
    //The number of spanning trees in K_n is num_elements**(num_elements-2) by Cayley's formula
    for(unsigned int i=10; i<=12; i++)
        test(0, i, ipow<uint64_t>(i, i-2));
}

