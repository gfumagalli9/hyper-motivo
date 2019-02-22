/*
 * SimpleGraph.h
 *
 *  Created on: 24 set 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_GRAPH_SIMPLEGRAPH_H_
#define SRC_COMMON_GRAPH_SIMPLEGRAPH_H_

#include <set>
#include <sparsehash/dense_hash_set>
#include <algorithm>
#include "UndirectedGraph.h"
#include "../treelets/Treelet.h"

/**
 * A simple graph(let) that can hold at most 16 vertices
 */
class SimpleGraph
{
public:
	typedef google::dense_hash_set<Treelet::treelet_structure_t> treelet_structure_set_t;

private:
	unsigned int nverts=0;
	unsigned int degrees[16] = {0};
	unsigned int adj_lists[16][16] = {{0}};

	Treelet dfs(unsigned int u, unsigned int parent, bool *visited, treelet_structure_set_t &structures);

public:
	unsigned int number_of_vertices() { return nverts; };

	void decompose(treelet_structure_set_t &structures, int root=-1);

	static SimpleGraph from_stdin();

	static SimpleGraph path(unsigned int size)
	{
		SimpleGraph g;
	    g.nverts=size;
	    for(unsigned int i=1; i<size; i++)
	    {
	        g.adj_lists[i-1][g.degrees[i-1]++]=i;
	        g.adj_lists[i][g.degrees[i]++]=i-1;
	    }
	    return g;
	}

	static SimpleGraph star(unsigned int size)
	{
		SimpleGraph g;
	    g.nverts=size;
	    for(unsigned int i=1; i<size; i++)
	    {
	        g.adj_lists[0][g.degrees[0]++]=i;
	        g.adj_lists[i][g.degrees[i]++]=0;
	    }
	    return g;
	}
};

#endif /* SRC_COMMON_GRAPH_SIMPLEGRAPH_H_ */
