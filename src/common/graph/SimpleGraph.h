/*
 * SimpleGraph.h
 *
 *  Created on: 24 set 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_GRAPH_SIMPLEGRAPH_H_
#define SRC_COMMON_GRAPH_SIMPLEGRAPH_H_

#include "../treelets/Treelet.h"
#include <set>
#include <algorithm>
#include "UndirectedGraph.h"

class SimpleGraph;

/**
 * A simple graph(let) that can hold at most 16 vertices
 */
class SimpleGraph {
protected:
	unsigned int nverts=0;
	unsigned int degrees[16] = {0};
	unsigned int adj_lists[16][16] = {0};
public:
	SimpleGraph();
	~SimpleGraph();
	unsigned int n() { return nverts; };
	Treelet dfs(unsigned int u, unsigned int parent, bool *visited, std::set<Treelet> *treelets);
	void decompose(std::set<Treelet> *treelets, int root);

	static SimpleGraph from_stdin();

	static SimpleGraph from_treelet(Treelet& t);

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
