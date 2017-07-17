//
// Created by steven on 11/18/16.
//

#include "TreeletTableCollection.h"

TreeletTableCollection::TreeletTableCollection(const unsigned int capacity) : capacity(capacity), size(0)
{
    tables = new TreeletTable*[capacity];
    for(unsigned int i=1; i<=capacity; i++)
        tables[i-1]= nullptr;
}

TreeletTableCollection::TreeletTableCollection(const std::string& basename, const unsigned int size, const bool load_root_sampler, const unsigned int capacity) : capacity(capacity)
{
    tables = new TreeletTable*[capacity];
    for(unsigned int i=1; i<=capacity; i++)
        tables[i-1] = (i<=size)?new TreeletTable(basename+"."+std::to_string(i), load_root_sampler):nullptr;
}

TreeletTableCollection::~TreeletTableCollection()
{
    for(unsigned int i=0; i<capacity; i++)
        delete tables[i];

    delete[] tables;
}

void TreeletTableCollection::add(TreeletTable* table)
{
    tables[size++] = table;
}
