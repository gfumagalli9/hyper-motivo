#include <iostream>

#include "Graph.h"
#include "TreeletTable.h"
#include "TreeletTableCollection.h"

int main()
{
    Graph G("../../graphs/star.txt");
    std::cout << G.number_of_vertices() << " " << G.number_of_edges() << std::endl;

    GraphColoring coloring(G.number_of_vertices(), 5);
    TreeletTableCollection ttc(&G, &coloring, 5);
    ttc.fill();

    return 0;
}