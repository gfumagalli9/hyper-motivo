//
// Created by steven on 1/18/19.
//

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
