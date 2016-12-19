//
// Created by steven on 11/18/16.
//

#ifndef MOTIVO_TREELETTABLECOLLECTION_H
#define MOTIVO_TREELETTABLECOLLECTION_H


#include "UndirectedGraph.h"
#include "GraphColoring.h"
#include "TreeletTable.h"

class TreeletTableCollection
{
private:
    constexpr static int default_capacity = 16;
    const unsigned int capacity;
    unsigned int size;
    TreeletTable** tables;

    TreeletTableCollection(const TreeletTableCollection&) = delete;
    void operator=(const TreeletTableCollection&) = delete;

public:
    TreeletTableCollection(const unsigned int capacity=default_capacity);
    TreeletTableCollection(const std::string& basename, const unsigned int size, const unsigned int capacity=default_capacity);
    ~TreeletTableCollection();

    void add(TreeletTable* table);
    const TreeletTable* get_table(const unsigned int i) const { return tables[i-1]; };
};


#endif //MOTIVO_TREELETTABLECOLLECTION_H
