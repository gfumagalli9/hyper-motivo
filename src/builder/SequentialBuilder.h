//
// Created by steven on 12/21/18.
//

#ifndef MOTIVO_SEQUENTIAL_BUILDER_H
#define MOTIVO_SEQUENTIAL_BUILDER_H

#include "../common/graph/UndirectedGraph.h"
#include "ColorCodingBuilder.h"
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
                          const unsigned int size, const TreeletTableCollection* ttc, const bool store_only_0,
                          TreeletSelector* selector, std::ostream* output);
    void build [[gnu::hot]] ();
};


#endif //MOTIVO_SEQUENTIAL_BUILDER_H
