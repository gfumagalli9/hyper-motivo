// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef MOTIVO_TREELETTABLECOLLECTION_H
#define MOTIVO_TREELETTABLECOLLECTION_H


#include "../graph/UndirectedGraph.h"
#include "TreeletTable.h"

class TreeletTableCollection
{
private:
    constexpr static int default_capacity = 16;
    const unsigned int capacity;
    unsigned int size;
    TreeletTable** tables;

public:
    TreeletTableCollection(const TreeletTableCollection&) = delete;
    void operator=(const TreeletTableCollection&) = delete;

    explicit TreeletTableCollection(unsigned int capacity=default_capacity);

    ~TreeletTableCollection();

    void add(TreeletTable* table) { tables[size++] = table; }

    TreeletTable* get_table(const unsigned int i) const { return tables[i-1]; };
};


#endif //MOTIVO_TREELETTABLECOLLECTION_H
