//
// Created by steven on 11/18/16.
//

#include "TreeletTableCollection.h"

TreeletTableCollection::TreeletTableCollection(int capacity) : capacity(capacity), size(0)
{
    tables = new TreeletTable*[capacity];
    for(int i=1; i<=capacity; i++)
        tables[i-1]=NULL;
}

TreeletTableCollection::TreeletTableCollection(const std::string& basename, int size, int capacity) : capacity(capacity)
{
    tables = new TreeletTable*[capacity];
    for(int i=1; i<=capacity; i++)
        tables[i-1] = (i<=size)?new TreeletTable(basename+"."+std::to_string(i)):NULL;
}

TreeletTableCollection::~TreeletTableCollection()
{
    for(int i=0; i<capacity; i++)
        delete tables[i];

    delete[] tables;
}

void TreeletTableCollection::add(TreeletTable* table)
{
    tables[size++] = table;
}
