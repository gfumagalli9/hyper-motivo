#include "HyperSequentialBuilder.h"
#include "ColorCodingHashmap.h"

void HyperSequentialBuilder::build()
{
    UndirectedGraph::vertex_t num_verts = H->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(Hypergraph::vertex_t));

    ColorCodingHashmap table;

    for(Hypergraph::vertex_t u=from_vertex; u<=to_vertex; u++)
    {
        if (!store_only_0 || ttc->get_table(1)->begin(u).treelet().get_colors() == 1) //color 0 is represented as 1<<0 = 1
        {
            builder.combine(u, table);
        }

        std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(u, table, normalize);
        table.clear();

        output->write(to_write.first, static_cast<std::streamsize>(to_write.second));
        delete[] to_write.first;
    }
}

HyperSequentialBuilder::HyperSequentialBuilder(
    const Hypergraph*                H,
    Hypergraph::vertex_t             from_vertex,
    Hypergraph::vertex_t             to_vertex,
    unsigned int                     size,
    const TreeletTableCollection*    ttc,
    const TreeletTableCollection*    tIEc,
    bool                             store_only_0,
    TreeletStructureSelector*        selector,
    std::ostream*                    output,
    const bool                       normalize
)
  : H             (H)
  , from_vertex   (from_vertex)
  , to_vertex     (to_vertex)
  , size          (size)
  , ttc           (ttc)
  , tIEc          (tIEc) 
  , store_only_0  (store_only_0)
  , output        (output)
  , builder       (size, ttc, tIEc, selector)
  , normalize     (normalize)
{}