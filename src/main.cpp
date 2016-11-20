#include <iostream>

#include "Graph.h"
#include "TreeletTableBuilder.h"
#include "TreeletTableCollection.h"

int main()
{
    Graph G("../../graphs/star.txt");
    std::cout << G.number_of_vertices() << " " << G.number_of_edges() << std::endl;

    GraphColoring coloring(G.number_of_vertices(), 5);

    TreeletTableCollection ttc;

    for(int i = 1; i <= 5; i++)
    {
        {
            TreeletTableBuilder builder(&G, &coloring, i, &ttc);
            builder.build();
            builder.write("test." + std::to_string(i));
        }
        ttc.add(new TreeletTable("test." + std::to_string(i)));
    }

    return 0;
}