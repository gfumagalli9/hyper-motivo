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

#ifndef MOTIVO_SEQUENTIAL_BUILDER_H
#define MOTIVO_SEQUENTIAL_BUILDER_H

#include "ColorCodingBuilder.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/io/ConcurrentWriter.h"

class SequentialBuilder
{
private:
    const UndirectedGraph* const G;
    const UndirectedGraph::vertex_t from_vertex;
    UndirectedGraph::vertex_t to_vertex;
    const unsigned int size;
    const TreeletTableCollection* const ttc;
    const bool store_only_0;
    std::ostream* const output;
    ColorCodingBuilder builder;

public:
    SequentialBuilder(const UndirectedGraph* G, UndirectedGraph::vertex_t from_vertex, UndirectedGraph::vertex_t to_vertex,
                          unsigned int size, const TreeletTableCollection* ttc, bool store_only_0,
                          TreeletStructureSelector* selector, std::ostream* output);

    void build [[gnu::hot]] ();
};


#endif //MOTIVO_SEQUENTIAL_BUILDER_H
