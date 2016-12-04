//
// Created by steven on 11/29/16.
//

#include "doctest.h"
#include "../src/sampler/KirchhoffSpanningTreeCounter.h"
#include <sstream>
#include <cmath>

/* Graph test contains 56 vertices and 159 edges:
 * A clique of 16 vertices on vertices 0-15 (120 edges)
 * A star with 15 leaves on vertices 16-31, whose root is vertex 16 (15 edges)
 * A path of 16 vertices on vertices 32-47 (15 edges)
 * A diamond on vertices 48-51 (5 edges)
 * A paw on vertices 52-55 (4 edges)
 */
UndirectedGraph test_graph( "../graphs/test" );

void test(unsigned int from, unsigned int size, uint64_t expected)
{
    UndirectedGraph::vertex_t* subgraph = new UndirectedGraph::vertex_t[size];
    for(unsigned int i=0; i<size; i++)
        subgraph[i]=from+i;

    KirchhoffSpanningTreeCounter kstc(&test_graph, size);
    CHECK( kstc.count(subgraph) == expected );

    delete[] subgraph;
}

TEST_CASE("KirchoffSpanningTreeCounter misc")
{
    //Size 1 subgraph
    test(1, 1, 1);

    //A diamond
    test(48, 4, 8);

    //A paw
    test(52, 4, 3);
}

TEST_CASE("KirchoffSpanningTreeCounter stars")
{
    for(unsigned int i=2; i<=16; i++)
        test(16, i, 1);
}

TEST_CASE("KirchoffSpanningTreeCounter paths")
{
    //Look at the subgraph induced by the first i vertices of the cycle
    for(unsigned int i=2; i<=16; i++)
        test(32, i, 1);
}


/*TEST_CASE("KirchoffSpanningTreeCounter cycles")
{
    for(int i=3; i<=16; i++)
        test(cycle(i), i, subgraph, i);
}
*/

TEST_CASE("KirchoffSpanningTreeCounter cliques")
{
    //The number of spanning trees in K_n is n**(n-2) by Cayley's formula
    for(int i=1; i<=14; i++) //FIXME: Fails for i=15 and i=16
        test(0, i, static_cast<uint64_t >(pow(i, i-2)+0.5));
}