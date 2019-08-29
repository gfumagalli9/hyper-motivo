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

#ifndef MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H
#define MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H

#include <unordered_map>
#include "Occurrence.h"
#include "../common/platform/platform.h"
#include "../common/treelets/Treelet.h"
#include "../builder/ColorCodingBuilder.h"
#include "../common/treelets/TreeletStructureSelector.h"

class ColorCodingSpanningTreeCounter
{
public:
    //The number of spanning trees in a complete graph of 16 vertices is 16^14.
    //Considering overcounting we get values thar are <= 16^15 < 2^(15 log 16) = 2^60
	typedef std::unordered_map<Treelet, uint64_t , Treelet::TreeletHash> table_t;

private:

//For ease of switching the Hashmap implementation.
// std::unordered_map requires no special initialization, therefore the following macro is empty
#define COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(hm)

    const Occurrence *occurrence;
    const unsigned int size;
    const bool store_only_0;
    const TreeletStructureSelector *selector;
    table_t **tables = nullptr;

    void do_build(unsigned int current_size);
    void combine(unsigned int u, unsigned int v, unsigned int current_size);

public:
    explicit ColorCodingSpanningTreeCounter(const Occurrence* occurrence, bool store_only_0=false, const TreeletStructureSelector* selector=nullptr);

    ~ColorCodingSpanningTreeCounter();

    void count();
    uint64_t number_of_counted_rooted_spanning_trees() const;
    uint64_t number_of_counted_spanning_trees_rooted_at(unsigned int root) const;

    // get the spanning tree count table for a given root node
    inline const table_t &get_table(const unsigned int root) const
    {
    	return tables[size-1][root];
    }
};

#endif
