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
    constexpr static int default_capacity = 16;
    const int capacity;
    int size;
    TreeletTable** tables;

public:
    TreeletTableCollection(int capacity=default_capacity);
    TreeletTableCollection(const std::string& basename, int size, int capacity=default_capacity);
    ~TreeletTableCollection();

    void add(TreeletTable* table);
    const TreeletTable* get_table(int i) const { return tables[i-1]; };
};


#endif //MOTIVO_TREELETTABLECOLLECTION_H
