//
// Created by steven on 12/3/16.
//

#include "../common/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "include_nauty.h"
#include "GraphFootprint.h"

int main()
{
    UndirectedGraph G("../graphs/star");
    TreeletTableCollection ttc("test", 5);

    TreeletSampler sampler(&G, &ttc);
    UndirectedGraph::vertex_t occurrence[10];

    //FIXME: Cache spanning trees count and/or footprints?

    GraphFootprint footprint;
    for(unsigned int i = 1; i <= 10000; i++)
    {
        sampler.sample(5, occurrence);
        for(int j = 0; j < 5; j++)
            std::cout << occurrence[j] << "\t";;

        GraphFootprint::footprint f = footprint.get_footprint(&G, occurrence, 5);
        std::cout << f.to_string() << std::endl;
    }
}