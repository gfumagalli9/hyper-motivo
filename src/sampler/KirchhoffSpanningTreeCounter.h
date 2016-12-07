//
// Created by steven on 11/29/16.
//

#ifndef MOTIVO_KIRCHHOFFSPANNINGTREECOUNTER_H
#define MOTIVO_KIRCHHOFFSPANNINGTREECOUNTER_H


#include "../common/UndirectedGraph.h"

class KirchhoffSpanningTreeCounter
{
private:
    const UndirectedGraph* graph;
    const unsigned int size;
    double* matrix;

public:
    KirchhoffSpanningTreeCounter(const UndirectedGraph* graph, const unsigned int size);
    ~KirchhoffSpanningTreeCounter();

    ///@returns the number of spanning tree of the subgraph of the associated graph induced by the vertices in @param subgraph
    ///@param subgraph must be a vector of the size specified when this object was constructed.
    ///The induced subgraph must be connected
    uint64_t count(const UndirectedGraph::vertex_t* subgraph);
};


#endif //MOTIVO_KIRCHHOFFSPANNINGTREECOUNTER_H
