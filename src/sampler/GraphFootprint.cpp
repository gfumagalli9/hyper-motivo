//
// Created by steven on 12/3/16.
//

#include "GraphFootprint.h"
#include "include_nauty.h"


GraphFootprint::footprint GraphFootprint::get_footprint(const UndirectedGraph *graph, const UndirectedGraph::vertex_t *subgraph, unsigned int size)
{
    nauty_graph g[MOTIVO_NAUTY_MAXN*MOTIVO_NAUTY_MAXM];
    nauty_graph cang[MOTIVO_NAUTY_MAXN*MOTIVO_NAUTY_MAXM];

    static DEFAULTOPTIONS_GRAPH(options);
    options.getcanon = MOTIVO_NAUTY_TRUE;

    int m = SETWORDSNEEDED(static_cast<int>(size));

#ifndef NDEBUG
    nauty_check(MOTIVO_NAUTY_WORDSIZE, m, static_cast<int>(size), NAUTYVERSIONID);
#endif

    EMPTYGRAPH(g, static_cast<unsigned int>(m), size);
    for(unsigned int i=1; i<size; i++)
    {
        UndirectedGraph::vertex_t u=subgraph[i];
        for(unsigned int j=0; j<i; j++)
        {
            UndirectedGraph::vertex_t v=subgraph[j];
            if(graph->has_edge(u,v))
                ADDONEEDGE(g, static_cast<int>(i), static_cast<int>(j), static_cast<unsigned int>(m));
        }
    }

    int lab[MOTIVO_NAUTY_MAXN];
    int ptn[MOTIVO_NAUTY_MAXN];
    int orbits[MOTIVO_NAUTY_MAXN];
    statsblk stats;
    densenauty(g, lab, ptn, orbits, &options, &stats, m, static_cast<int>(size), cang);

    GraphFootprint::footprint f{};
    int n=0;
    for(unsigned int i=0; i<size; i++)
    {
        nauty_set* row = GRAPHROW (cang ,i, static_cast<unsigned int>(m));
        for(unsigned int j = 0; j <= i; j++)
        {
            n++;
            if( ISELEMENT( row , j ) )
            {
                f.data[ n/8 ] |= static_cast<uint8_t>(0b10000000 >> (n%8));
            }
        }
    }

    return f;
}

std::string GraphFootprint::footprint::to_string()
{
    char c[32];
    for(int i=0; i<16; i++)
    {
        c[2*i]= static_cast<char>('A'+ (data[i]>>4));
        c[2*i+1]= static_cast<char>('A'+ (data[i] & 0x0F));
    }

    return std::string(c, 32);
}
