//
// Created by steven on 11/18/16.
//

#ifndef MOTIVO_TREELETTABLECOLLECTION_H
#define MOTIVO_TREELETTABLECOLLECTION_H


#include "Graph.h"
#include "GraphColoring.h"
#include "TreeletTable.h"

class TreeletTableCollection
{
private:
    TreeletTable** tables;
    const int max_size;

public:
    TreeletTableCollection(Graph* graph, GraphColoring* coloring, int max_size);
    ~TreeletTableCollection();

    void fill();

};


#endif //MOTIVO_TREELETTABLECOLLECTION_H
