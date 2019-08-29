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

#ifndef MOTIVO_SPANNINGTREECOUNTER_H
#define MOTIVO_SPANNINGTREECOUNTER_H

#include <cstdint>
#include "Occurrence.h"
#include "../common/treelets/TreeletStructureSelector.h"

class SpanningTreeCounter
{
public:
    typedef unsigned int strategy_t;
    static constexpr strategy_t STRATEGY_STARS=0;
    static constexpr strategy_t STRATEGY_KIRCHOFF=1;
    static constexpr strategy_t STRATEGY_KIRCHOFF_MINUS_STARS=2;
    static constexpr strategy_t STRATEGY_COLOR_CODING=3;
    static constexpr strategy_t STRATEGY_ZERO=4;

private:
    const unsigned int size;
    strategy_t strategy;
    const TreeletStructureSelector *selector;

public:
    static uint64_t number_of_rooted_spanning_trees_kirchhoff(const Occurrence &occ);

    static uint64_t number_of_rooted_spanning_trees_colorcoding(const Occurrence &occ, const TreeletStructureSelector *ts = nullptr);

    static unsigned int number_of_rooted_spanning_stars(const Occurrence &occ);

    ///Instance methods. Choose a good strategy for the given size an selector.
    ///The spanning trees to be counted are those of the given size than can be obtained by a build that uses @param selector
    explicit SpanningTreeCounter(unsigned  int size, const TreeletStructureSelector *selector=nullptr);

    strategy_t get_strategy() const { return strategy; }

    uint64_t number_of_rooted_spanning_trees(const Occurrence &occ);
};


#endif //MOTIVO_SPANNINGTREECOUNTER_H
