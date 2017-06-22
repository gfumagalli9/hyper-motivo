//
// Created by steven on 12/18/16.
//

#include <lapacke.h>

#include "Occurrence.h"
#include "include_nauty.h"

Occurrence::Occurrence(const unsigned int size, const UndirectedGraph::vertex_t *occ, const UndirectedGraph *graph) : size(size)
{
    for(unsigned int i=0; i<size; i++)
        verts[i]=occ[i];

    for(unsigned int i=1; i<size; i++)
    {
        for(unsigned int j=0; j<i; j++)
        {
            if(graph->has_edge(verts[i], verts[j]))
                add_edge(i,j);
        }
    }
}

Occurrence::Occurrence(const UndirectedGraph::vertex_t *occ, const Treelet& treelet) : size(treelet.number_of_vertices())
{
    for(unsigned int i=0; i<size; i++)
        verts[i]=occ[i];

    unsigned int parents[16] = {0};
    unsigned int current = 0;
    unsigned int n=0;
    for(Treelet::treelet_structure_t structure = treelet.get_structure(); structure; structure<<=1)
    {
        if(structure & Treelet::treelet_structure_highest_bit)
        {
            n++;
            add_edge(n, current);
            parents[n]=current;
            current=n;
        }
        else
            current=parents[current];
    }

    assert(n==size-1);
}

std::string Occurrence::text_footprint()
{
    char c[text_footprint_bytes];
    for(unsigned int i=0; i<binary_footprint_bytes; i++)
    {
        c[2*i]= static_cast<char>('A'+ (edges[i]>>4));
        c[2*i+1]= static_cast<char>('A'+ (edges[i] & 0x0F));
    }

    return std::string(c, text_footprint_bytes);
}

std::string Occurrence::to_string()
{
    std::string s = text_footprint();
    for(unsigned int i=0; i<size; i++)
        s += " " + std::to_string(verts[i]);

    return s;
}

void Occurrence::canonicize()
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
        for(unsigned int j=0; j<i; j++)
        {
            if(has_edge(i,j))
                ADDONEEDGE(g, static_cast<int>(i), static_cast<int>(j), static_cast<unsigned int>(m));
        }
    }

    int lab[MOTIVO_NAUTY_MAXN];
    int ptn[MOTIVO_NAUTY_MAXN];
    int orbits[MOTIVO_NAUTY_MAXN];
    statsblk stats;
    densenauty(g, lab, ptn, orbits, &options, &stats, m, static_cast<int>(size), cang);

    //From the nauty manual: the value of lab on return is the canonical labelling
    //of the graph. Precisely, it lists the vertices of g in the order in which they need to
    //be relabelled to give canong

    UndirectedGraph::vertex_t verts[16];
    memcpy(verts, verts, sizeof(UndirectedGraph::vertex_t)*size);

    for(unsigned int i=1; i<size; i++)
        verts[i] = verts[ lab[i] ];

    memset(edges, 0, sizeof(uint8_t)*size);
    for(unsigned int i=1; i<size; i++)
    {
        nauty_set* row = GRAPHROW (cang ,i, static_cast<unsigned int>(m));
        for(unsigned int j = 0; j < i; j++)
        {
            if( ISELEMENT( row , j ) )
                add_edge(i, j);
         }
    }
}

uint64_t Occurrence::number_of_spanning_trees()
{
    //Handle small cases
    if(size<=2) //Isolated vertex or 2 vertices and a single edge
        return 1;

    if(size==3)
    {
        //Triangle?
        if( has_edge(1,0) && has_edge(2,1) && has_edge(2,0) )
            return 3;

        return  1;
    }

    double* matrix = new double[(size-1)*(size-1)];

    //Compute the num_vertices-1 x num_vertices-1 submatrix of the Laplacian matrix of the subgraph of G induced by "subgraph"
    unsigned int nedges=0;
    for(unsigned int i=0; i<size-1; i++)
    {
        matrix[i*size] = 0; //i*num_vertices == i*(num_vertices-1)+i
        for(unsigned int j=0; j<i; j++)
        {
            if(has_edge(i, j))
            {
                matrix[i*size]++; //Involves only elements already set to 0
                matrix[j*size]++; //Ditto
                matrix[i*(size-1)+j]=-1;
                nedges++;
            }
            else
                matrix[i*(size-1)+j]=0;
        }
    }

    //The degress of the Laplacian matrix are not accounting  for the edges incident to the last vertex of the subgraph, add them
    for(unsigned int j=0; j<size-1; j++)
    {
        if(has_edge(size - 1, j))
        {
            matrix[j*size]++;
            nedges++;
        }
    }

    if(nedges==size-1) //The subgraph is a tree
    {
        delete[] matrix;
        return 1;
    }

    //Compute the lower factor L of a  Cholesky factorization
    //L is stored in the lower triangular part of matrix
#ifndef NDEBUG
    int r =
#endif
    LAPACKE_dpotrf_work(LAPACK_ROW_MAJOR, 'L', static_cast<int>(size-1), matrix, static_cast<int>(size-1));
    assert(r==0);

    //The determinant is the product of the squares of the elements on the diagonal of L
    double det = 1;
    for(unsigned int i=0; i<size-1; i++)
        det*=matrix[i*size];

    delete[] matrix;

    return static_cast<uint64_t>(det*det + 0.5); //fast round(det*det)
}
