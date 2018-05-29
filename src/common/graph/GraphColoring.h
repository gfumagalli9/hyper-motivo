//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_GRAPHCOLORING_H
#define MOTIVO_GRAPHCOLORING_H

#include <cstdint>
#include "UndirectedGraph.h"
#include "../Random.h"

class GraphColoring {
public:
	typedef uint8_t color_t;

protected:
	color_t* colors;
	const UndirectedGraph::vertex_t from;
#ifndef NDEBUG
	const UndirectedGraph::vertex_t to;
#endif

	GraphColoring() :
			from(0)
#ifndef NDEBUG
					, to(0)
#endif
	{
		colors = NULL;
	}
	GraphColoring(const GraphColoring&) = delete;
	void operator=(const GraphColoring&) = delete;

public:
	///Contructs a random coloring of @param n vertices using @param number_of_colors colors
	GraphColoring(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to,
			unsigned int number_of_colors, Random* rng);

	~GraphColoring();

	///@returns the color of vertex @param u
	color_t color_of(long u) const {
		assert(u >= from);
		assert(u <= to);
		return colors[u - from];
	}
	;
};

#endif //MOTIVO_GRAPHCOLORING_H
