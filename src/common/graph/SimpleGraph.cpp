/*
 * SimpleGraph.cpp
 *
 *  Created on: 24 set 2018
 *      Author: brix
 */

#include "SimpleGraph.h"
#include <stack>

SimpleGraph::SimpleGraph() {
	// TODO Auto-generated constructor stub
}

SimpleGraph::~SimpleGraph() {
	// TODO Auto-generated destructor stub
}

Treelet SimpleGraph::dfs(unsigned int u, unsigned int parent, bool *visited, std::set<Treelet> *treelets)
{
    visited[u]=true;

    int nchild_treelets=0;
    Treelet child_treelets[15];

    for(unsigned int i=0; i<degrees[u]; i++)
    {
        unsigned int v = adj_lists[u][i];
        if(parent==v)
            continue;

        if(visited[v])
            throw std::runtime_error("Graph is not a tree");

        child_treelets[nchild_treelets++] = dfs(v, u, visited, treelets);
    }

    std::sort(child_treelets, child_treelets+nchild_treelets);

    Treelet t = Treelet::singleton(static_cast<uint8_t>(u));
    while(nchild_treelets!=0)
    {
        t=t.merge(child_treelets[--nchild_treelets]);
        assert(t.is_valid());
        treelets->insert(t);
    }

    return t;
}

/**
 * Returns a set of treelets
 */
void SimpleGraph::decompose(std::set<Treelet> *treelets, int root)
{
    bool visited[16];

    if(root==-1)
    {
        for(UndirectedGraph::vertex_t u=0; u<16; u++)
        {
            memset(visited, 0, sizeof(bool)*16);
            dfs(u, u, visited, treelets);
        }
    }
    else
    {
        memset(visited, 0, sizeof(bool)*16);
        dfs(0, 0, visited, treelets);
    }
}

SimpleGraph SimpleGraph::from_stdin() {
	SimpleGraph g;
    bool seen[16]={0};
    unsigned int nedges=0;

    unsigned int  u,v;
    while(std::cin >> u >> v)
    {
        if(u>=16 || v>=16 || u==v)
        {
            std::cerr << "Invalid edge" << std::endl;
            continue;
        }

        bool existing=false;
        for(unsigned int j=0; j<=g.degrees[u]; j++)
            existing |= (g.adj_lists[u][j]==v);

        if(existing)
        {
            std::cerr << "Duplicate edge" << std::endl;
            continue;
        }

        if(!seen[u])
        {
            seen[u]=true;
            g.nverts++;
        }

        if(!seen[v])
        {
            seen[v]=true;
            g.nverts++;
        }

        g.adj_lists[u][g.degrees[u]++]=v;
        g.adj_lists[v][g.degrees[v]++]=u;
        nedges++;
    }

    for(unsigned int i=0; i<g.nverts; i++)
    {
        if(!seen[i])
            throw std::runtime_error("Vertex IDs are not contiguous");
    }

    if(nedges!=g.nverts-1)
        throw std::runtime_error("Graph is not a tree");

    return g;
}


SimpleGraph SimpleGraph::from_treelet(Treelet& t) {
	SimpleGraph g;
	Treelet::treelet_structure_t tst = t.get_structure();
	g.nverts = t.number_of_vertices();
	std::stack<int> s;
	int maxu = 0;
	s.push(maxu);
	for (int i = 0; i < t.number_of_vertices(); i++) {
		if (tst & 0x1) {
			g.adj_lists[s.top()][g.degrees[s.top()]++] = ++maxu;
			s.push(maxu);
		} else
			s.pop();
		tst >>= 1;
	}
	return g;
}



