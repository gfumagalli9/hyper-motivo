/*
 * SimpleGraph.cpp
 *
 *  Created on: 24 set 2018
 *      Author: brix
 */

#include "SimpleGraph.h"
#include <stack>


Treelet SimpleGraph::dfs(unsigned int u, unsigned int parent, bool *visited, treelet_structure_set_t &structures)
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

        child_treelets[nchild_treelets++] = dfs(v, u, visited, structures);
    }

    std::sort(child_treelets, child_treelets+nchild_treelets);

    Treelet t = Treelet::singleton(static_cast<uint8_t>(u));
    while(nchild_treelets!=0)
    {
        t=t.merge(child_treelets[--nchild_treelets]);
        assert(t.is_valid());
        structures.insert(t.get_structure());
    }

    return t;
}

/**
 * Return all (sub)treelets found by DFS from the different nodes of the graph, of different sizes.
 * If the graph is a tree, this returns all possible rootings of the tree itself.
 */
void SimpleGraph::decompose(treelet_structure_set_t &structures, int root)
{
    bool visited[16];
    if(root==-1)
    {
        for(UndirectedGraph::vertex_t u=0; u<16; u++)
        {
            memset(visited, 0, sizeof(bool)*16);
            dfs(u, u, visited, structures);
        }
    }
    else
    {
        memset(visited, 0, sizeof(bool)*16);
        dfs(0, 0, visited, structures);
    }
}

SimpleGraph SimpleGraph::from_stdin() { //TODO: Does this belong here? Also, it only reads trees.
	SimpleGraph g;
    bool seen[16]={false};
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




