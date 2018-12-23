//
// Created by steven on 12/23/18.
//

#include "Size1ColorCoding.h"

void Size1ColorCoding::build()
{
    output->write(reinterpret_cast<const char*>(&number_of_vertices), sizeof(UndirectedGraph::vertex_t));

    constexpr std::streamsize buf_size = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + sizeof(TreeletTable::treelet_count_pair);
    char buffer[buf_size];

    constexpr uint64_t one=1;
    memcpy(buffer+sizeof(UndirectedGraph::vertex_t), &one, sizeof(uint64_t));

    TreeletTable::treelet_count_pair tcp;
    tcp.count=1;

    for (UndirectedGraph::vertex_t u = from_vertex; u<=to_vertex; u++)
    {
        uint8_t color = static_cast<uint8_t>(rng->random_uint(0, number_of_colors-1));
        if (store_only_0 && color != 0)
            continue;

        memcpy(buffer, &u, sizeof(UndirectedGraph::vertex_t));
        tcp.treelet = Treelet::singleton(color);
        memcpy(buffer + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t), &tcp, sizeof(TreeletTable::treelet_count_pair));
        output->write(buffer, buf_size);
    }
}

Size1ColorCoding::Size1ColorCoding(UndirectedGraph::vertex_t number_of_vertices, UndirectedGraph::vertex_t from_vertex,
                                   UndirectedGraph::vertex_t to_vertex, uint8_t number_of_colors, bool store_only_0, Random *rng,
                                   std::ostream *output)
        : number_of_vertices(number_of_vertices), from_vertex(from_vertex), to_vertex(to_vertex), number_of_colors(number_of_colors),
          store_only_0(store_only_0), rng(rng), output(output)
{
    if(number_of_colors<=1)
        throw std::runtime_error("Invalid number of colors");
}
