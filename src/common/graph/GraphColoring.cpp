//
// Created by steven on 11/13/16.
//

#include "GraphColoring.h"

GraphColoring::GraphColoring(const UndirectedGraph::vertex_t from,
		const UndirectedGraph::vertex_t to, unsigned int number_of_colors, Random* rng) :
		from(from)
#ifndef NDEBUG
				, to(to)
#endif
{
	assert(to >= from);
	UndirectedGraph::vertex_t size = to - from + 1;
	colors = new color_t[size];
	for (UndirectedGraph::vertex_t u = 0; u < size; u++)
		colors[u] = static_cast<color_t>(rng->random_uint(0u, number_of_colors - 1));
}

GraphColoring::~GraphColoring() {
	delete[] colors;
}
