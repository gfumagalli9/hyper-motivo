/*
 * FullGraphColoring.h
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_GRAPH_FULLGRAPHCOLORING_H_
#define SRC_COMMON_GRAPH_FULLGRAPHCOLORING_H_

#include "GraphColoring.h"

/**
 * Gives a distinct color to each node of the graph. It actually returns the id of a node as its color.
 */
class FullGraphColoring: public GraphColoring {
public:
	FullGraphColoring(const GraphColoring&) = delete;
	void operator=(const GraphColoring&) = delete;
	FullGraphColoring();
	~FullGraphColoring();
	color_t color_of(long u) const {
		std::cout << "returning color " << u << std::endl;
		return static_cast<color_t>(u);
	}
};

#endif /* SRC_COMMON_GRAPH_FULLGRAPHCOLORING_H_ */
