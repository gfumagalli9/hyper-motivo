//
// Created by steven on 11/18/16.
//

#include "TreeletTableCollection.h"

TreeletTableCollection::TreeletTableCollection(Graph *graph, GraphColoring *coloring, int max_size) : max_size(max_size)
{
    tables = new TreeletTable*[max_size];
    for(int i=1; i<=max_size; i++)
        tables[i-1] = new TreeletTable(graph, coloring, i, (i==1)?NULL:tables);
}

void TreeletTableCollection::fill()
{
    for(int i=0; i<max_size; i++)
        tables[i]->fill();
}

TreeletTableCollection::~TreeletTableCollection()
{
    for(int i=0; i<max_size; i++)
        delete tables[i];

    delete[] tables;
}
