// MIT License
#include "SequentialBuilder.h"
#include "ColorCodingHashmap.h"

void SequentialBuilder::build()
{
    UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    ColorCodingHashmap table;
    for (UndirectedGraph::vertex_t u = from_vertex; u <= to_vertex; ++u)
    {
        // color 0 è rappresentato da 1<<0 = 1
        if (!store_only_0 || ttc->get_table(1)->begin(u).treelet().get_colors() == 1)
        {
            const UndirectedGraph::vertex_t degree = G->degree(u);
            for (UndirectedGraph::vertex_t d = 0; d < degree; ++d) {
                const auto v = G->neighbor(u, d);
                // NESSUN filtro su PairSet: il LOW è già filtrato in preprocess
                builder.combine(u, v, table);
            }
        }

        std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(u, table, normalize);
        table.clear();

        output->write(to_write.first, static_cast<std::streamsize>(to_write.second));
        delete[] to_write.first;
    }
}

SequentialBuilder::SequentialBuilder(const UndirectedGraph *G,
                                     UndirectedGraph::vertex_t from_vertex,
                                     UndirectedGraph::vertex_t to_vertex,
                                     const unsigned int size,
                                     const TreeletTableCollection *ttc,
                                     const bool store_only_0,
                                     TreeletStructureSelector *selector,
                                     std::ostream *output,
                                     const bool normalize)
    : G(G)
    , from_vertex(from_vertex)
    , to_vertex(to_vertex)
    , size(size)
    , ttc(ttc)
    , store_only_0(store_only_0)
    , output(output)
    , builder(size, ttc, selector)
    , normalize(normalize)
{}