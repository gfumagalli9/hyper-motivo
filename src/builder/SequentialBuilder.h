// MIT License
#ifndef MOTIVO_SEQUENTIAL_BUILDER_H
#define MOTIVO_SEQUENTIAL_BUILDER_H

#include "ColorCodingBuilder.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/io/ConcurrentWriter.h" // opzionale; puoi rimuoverlo se inutilizzato

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
    const bool normalize;

public:
    SequentialBuilder(const UndirectedGraph* G,
                      UndirectedGraph::vertex_t from_vertex,
                      UndirectedGraph::vertex_t to_vertex,
                      unsigned int size,
                      const TreeletTableCollection* ttc,
                      bool store_only_0,
                      TreeletStructureSelector* selector,
                      std::ostream* output,
                      const bool normalize);

    void build [[gnu::hot]] ();
};

#endif // MOTIVO_SEQUENTIAL_BUILDER_H