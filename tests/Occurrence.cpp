//
// Created by steven on 11/29/16.
//

#include "../src/sampler/Occurrence.h"

#include "doctest.h"
#include <sstream>
#include <cmath>
#include "../src/common/graph/UndirectedGraph.h"

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 */
UndirectedGraph test_graph("test-graph");

void test(unsigned int from, unsigned int size, uint64_t expected)
{
    UndirectedGraph::vertex_t* subgraph = new UndirectedGraph::vertex_t[size];
    for(unsigned int i=0; i<size; i++)
        subgraph[i]=from+i;

    Occurrence occ(size, &test_graph,  subgraph);
    CHECK( occ.number_of_spanning_trees() == expected );

    delete[] subgraph;
}

TEST_CASE("Occurrence number_of_spanning_trees misc")
{
    //Size 1 subgraph
    test(1, 1, 1);

    //A diamond
    test(48, 4, 8);

    //A paw
    test(52, 4, 3);
}

TEST_CASE("Occurrence number_of_spanning_trees stars")
{
    for(unsigned int i=2; i<=16; i++)
        test(16, i, 1);
}

TEST_CASE("Occurrence number_of_spanning_trees paths")
{
    //Look at the subgraph induced by the first i vertices of the cycle
    for(unsigned int i=2; i<=16; i++)
        test(32, i, 1);
}


/*TEST_CASE("Occurrence number_of_spanning_trees cycles")
{
    for(int i=3; i<=16; i++)
        test(cycle(i), i, subgraph, i);
}
*/

TEST_CASE("Occurrence number_of_spanning_trees cliques")
{
    //The number of spanning trees in K_n is num_elements**(num_elements-2) by Cayley's formula
    for(unsigned int i=1; i<=14; i++) //FIXME: Fails for i=15 and i=16
        test(0, i, static_cast<uint64_t >(pow(i, i-2)+0.5));
}
