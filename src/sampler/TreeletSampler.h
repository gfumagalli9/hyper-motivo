//
// Created by steven on 11/27/16.
//

#ifndef MOTIVO_TREELETSAMPLER_H
#define MOTIVO_TREELETSAMPLER_H

#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTableCollection.h"

class TreeletSampler
{
private:
    const UndirectedGraph* graph;
    const TreeletTableCollection* table_collection;
    Random *rng;

public:
    TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, Random* rng) : graph(graph), table_collection(ttc), rng(rng) {};

    ///Samples an occurrence of @param t rooted in @param u
    bool sample_rooted_occurrence [[gnu::hot]] (const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence);

    UndirectedGraph::vertex_t sample_root [[gnu::hot]] (const unsigned int size)
    {
        return table_collection->get_table(size)->get_random_root(rng);
    }

    Treelet sample_treelet [[gnu::hot]] (const unsigned int size, UndirectedGraph::vertex_t root)
    {
        Treelet t = table_collection->get_table(size)->get_random_treelet(root, rng);
        assert(t.is_valid());
        return t;
    }
};


#endif //MOTIVO_TREELETSAMPLER_H
