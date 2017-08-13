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

TreeletTableCollection::~TreeletTableCollection()
{
    delete[] tables;
}

