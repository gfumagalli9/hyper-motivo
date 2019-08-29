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

#include "SequentialBuilder.h"
#include "ColorCodingHashmap.h"

void SequentialBuilder::build()
{
    UndirectedGraph::vertex_t num_verts = G->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));

    ColorCodingHashmap table;
    for(UndirectedGraph::vertex_t u=from_vertex; u<=to_vertex; u++)
    {
        if (!store_only_0 || ttc->get_table(1)->begin(u).treelet().get_colors() == 1) //color 0 is represented as 1<<0 = 1
        {
            const UndirectedGraph::vertex_t degree = G->degree(u);
            for(UndirectedGraph::vertex_t d = 0; d < degree; d++)
                builder.combine(u, G->neighbor(u, d), table);
        }

        std::pair<char*, std::size_t> to_write = builder.to_normalized_sorted_byte_array(u, table);
        table.clear();

        output->write(to_write.first, static_cast<std::streamsize>(to_write.second));
        delete[] to_write.first;
    }
}

SequentialBuilder::SequentialBuilder(const UndirectedGraph *G, UndirectedGraph::vertex_t from_vertex,
                                             UndirectedGraph::vertex_t to_vertex, const unsigned int size,
                                             const TreeletTableCollection *ttc, const bool store_only_0,
                                             TreeletStructureSelector *selector, std::ostream *output)
        : G(G), from_vertex(from_vertex), to_vertex(to_vertex), size(size), ttc(ttc), store_only_0(store_only_0), output(output), builder(size, ttc, selector)

{}
