//
// Created by steven on 12/23/18.
//

#include "Size1Builder.h"
#include "../common/util.h"
#include <random>
#include <functional>

void Size1Builder::build()
{
    output->write(reinterpret_cast<const char*>(&number_of_vertices), sizeof(UndirectedGraph::vertex_t));

    constexpr std::streamsize buf_size = sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) + sizeof(TreeletTable::treelet_count_pair);
    char buffer[buf_size];

    constexpr uint64_t one=1;
    memcpy(buffer+sizeof(UndirectedGraph::vertex_t), &one, sizeof(uint64_t));

    TreeletTable::treelet_count_pair tcp;
    tcp.count=1;
/*  double col0prob = colprob_to_prob0(number_of_colors, bias);
    double *D = new double[number_of_colors];
    bimodal_distribution_find(D, number_of_colors, number_of_colors/2, bias);
    std::cout << pcold(D, number_of_colors) << std::endl;
    */
    std::hash<std::string> hash_fn;
    int nseed = hash_fn(rng->get_seed());
    std::cout << "Using random coloring seed " << nseed << std::endl;
    std::default_random_engine generator(nseed);
	std::discrete_distribution<int> cd(color_distribution, color_distribution + number_of_colors);

    for (UndirectedGraph::vertex_t u = from_vertex; u<=to_vertex; u++)
    {
        uint8_t color;
        color = cd(generator);
 /*       if (rng->random_uint<unsigned int>(1, 1e9) <= static_cast<int>(col0prob * 1e9))
        	color = 0;
        else
        	color = static_cast<uint8_t>(rng->random_uint(1, number_of_colors-1));*/
        if (store_only_0 && color != 0)
            continue;

        memcpy(buffer, &u, sizeof(UndirectedGraph::vertex_t));
        tcp.treelet = Treelet::singleton(color);
        memcpy(buffer + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t), &tcp, sizeof(TreeletTable::treelet_count_pair));
        output->write(buffer, buf_size);
    }
}

Size1Builder::Size1Builder(UndirectedGraph::vertex_t number_of_vertices, UndirectedGraph::vertex_t from_vertex,
                                   UndirectedGraph::vertex_t to_vertex, uint8_t number_of_colors, bool store_only_0,
                                   double bias, double *color_distribution, Random *rng, std::ostream *output)
        : number_of_vertices(number_of_vertices), from_vertex(from_vertex), to_vertex(to_vertex), number_of_colors(number_of_colors),
          store_only_0(store_only_0), bias(bias), color_distribution(color_distribution), rng(rng), output(output)
{
    if(number_of_colors<=1)
        throw std::runtime_error("Invalid number of colors");

    if(bias==0)
        throw std::runtime_error("Invalid bias");
}
