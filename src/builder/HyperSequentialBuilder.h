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

#ifndef MOTIVO_HYPER_SEQUENTIAL_BUILDER_H
#define MOTIVO_HYPER_SEQUENTIAL_BUILDER_H

#include "InclusionExclusionBuilder.h"
#include "../common/graph/Hypergraph.h"
#include "../common/io/ConcurrentWriter.h"

class HyperSequentialBuilder
{
private:
    const Hypergraph* const H;
    const Hypergraph::vertex_t from_vertex;
    Hypergraph::vertex_t to_vertex;
    const unsigned int size;
    const TreeletTableCollection* const ttc;
    const TreeletTableCollection* const tIEc;
    const bool store_only_0;
    std::ostream* const output;
    IEBuilder builder;
    const bool normalize;

public:
    HyperSequentialBuilder(const Hypergraph* H, Hypergraph::vertex_t from_vertex, Hypergraph::vertex_t to_vertex,
                          unsigned int size, const TreeletTableCollection* ttc, const TreeletTableCollection* tIEc, bool store_only_0,
                          TreeletStructureSelector* selector, std::ostream* output, const bool normalize);

    void build [[gnu::hot]] ();
};


#endif //MOTIVO_HYPER_SEQUENTIAL_BUILDER_H