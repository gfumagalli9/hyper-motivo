#include "HyperSequentialBuilder.h"

#include <cassert>

void HyperSequentialBuilder::build()
{
    // Write table header: number of vertices in the (HIGH) hypergraph.
    const Hypergraph::vertex_t num_verts = H->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts),
                  sizeof(Hypergraph::vertex_t));

    ColorCodingHashmap table; // map-like accumulator: Treelet -> count

    for (Hypergraph::vertex_t u = from_vertex; u <= to_vertex; ++u)
    {
        // If requested, only store rows whose root u has color 0.
        // (color 0 is encoded as bitmask 1, i.e., get_colors() == 1)
        if (!store_only_0 || ttc->get_table(1)->begin(u).treelet().get_colors() == 1){
            builder.combine(u, table);
        }

        // Serialize and write the per-vertex record.
        auto payload = builder.to_normalized_sorted_byte_array(u, table, normalize);
        table.clear();

        output->write(payload.first, static_cast<std::streamsize>(payload.second));
        delete[] payload.first;
    }

    // Caller owns the stream; just leave it in a good state.
    output->flush();
}

HyperSequentialBuilder::HyperSequentialBuilder(
    const Hypergraph*                 H,
    Hypergraph::vertex_t              from_vertex,
    Hypergraph::vertex_t              to_vertex,
    unsigned int                      size,
    const TreeletTableCollection*     ttc,
    const TreeletTableCollection*     tIEc,
    bool                              store_only_0,
    const TreeletStructureSelector*   selector,
    std::ostream*                     output,
    bool                              normalize
)
  : H            (H)
  , from_vertex  (from_vertex)
  , to_vertex    (to_vertex)
  , size         (size)
  , ttc          (ttc)
  , tIEc         (tIEc)
  , store_only_0 (store_only_0)
  , output       (output)
  , builder      (size, ttc, tIEc, selector)
  , normalize    (normalize)
{
    assert(H != nullptr);
    assert(output != nullptr);
    assert(size >= 2);            // this builder targets k >= 2
    assert(ttc != nullptr && tIEc != nullptr);
}