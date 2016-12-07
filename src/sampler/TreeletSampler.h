//
// Created by steven on 11/27/16.
//

#ifndef MOTIVO_TREELETSAMPLER_H
#define MOTIVO_TREELETSAMPLER_H

#include "../common/Treelet.h"
#include "../common/TreeletTableCollection.h"

class TreeletSampler
{
private:
    const UndirectedGraph* graph;
    const TreeletTableCollection* table_collection;
    Random rng;

public:
    TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc) : graph(graph), table_collection(ttc) {};

    ///Samples an occurrence of @param t rooted in @param u
    bool sample_rooted_occurrence(const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence);

    void sample(const unsigned int size, UndirectedGraph::vertex_t* occurrence);
};


#endif //MOTIVO_TREELETSAMPLER_H
