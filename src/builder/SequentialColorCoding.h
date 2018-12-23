//
// Created by steven on 12/21/18.
//

#ifndef MOTIVO_SEQUENTIAL_COLOR_CODING_H
#define MOTIVO_SEQUENTIAL_COLOR_CODING_H

#include "../common/graph/UndirectedGraph.h"
#include "ColorCodingBuilder.h"
#include "../common/io/ConcurrentWriter.h"

class SequentialColorCoding
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
    SequentialColorCoding(const UndirectedGraph* G, UndirectedGraph::vertex_t from_vertex, UndirectedGraph::vertex_t to_vertex, const unsigned int size, const TreeletTableCollection* ttc,
                          const bool store_only_0, TreeletSelector* selector, std::ostream* output)
            : G(G), from_vertex(from_vertex), to_vertex(to_vertex), size(size), ttc(ttc), store_only_0(store_only_0), output(output), builder(size, ttc, selector)
    {
    }

    void build()
    {
        UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
        output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

        for(UndirectedGraph::vertex_t u=from_vertex; u<=to_vertex; u++)
        {
            if (store_only_0 && ttc->get_table(1)->begin(u).treelet().get_colors() != 1) //color 0 is represented as 1<<0 = 1
                continue;

            ColorCodingBuilder::table_t table;
            BUILDER_INIT_HASHMAP(table);
            const UndirectedGraph::vertex_t degree = G->degree(u);
            for (UndirectedGraph::vertex_t d = 0; d < degree; d++)
                builder.combine(u, G->neighbor(u, d), table);

            std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(u, table);
            output->write(to_write.first, static_cast<std::streamsize>(to_write.second));
            delete[] to_write.first;
        }
    }
};


#endif //MOTIVO_SEQUENTIAL_COLOR_CODING_H
