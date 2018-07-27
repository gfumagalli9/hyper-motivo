//
// Created by steven on 12/18/16.
//

#include <lapacke.h>

#include "Occurrence.h"

constexpr unsigned int Occurrence::binary_footprint_bits;
constexpr unsigned int Occurrence::binary_footprint_bytes;
constexpr unsigned int Occurrence::text_footprint_bytes;

Occurrence::Occurrence(const unsigned int size, const UndirectedGraph *graph, const UndirectedGraph::vertex_t *occ) : size(size)
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

Occurrence::Occurrence(const Treelet& treelet, const UndirectedGraph::vertex_t *occ) : size(treelet.number_of_vertices())
{
     for(unsigned int i = 0; i < size; i++)
            verts[i] = occ[i];

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

const char* Occurrence::text_footprint() const
{
    if(text_footprint_buffer[0]==0)
    {
        for(unsigned int i=0; i<binary_footprint_bytes; i++)
        {
            text_footprint_buffer[2*i]= static_cast<char>('A'+ (edges[i]>>4u));
            text_footprint_buffer[2*i+1]= static_cast<char>('A'+ (edges[i] & 0x0Fu));
        }
    }

    return text_footprint_buffer;
}

uint64_t Occurrence::number_of_spanning_trees() const
{
    if(spanning_trees!=0)
        return spanning_trees;

    //Handle small cases
    if(size<=2) //Isolated vertex or 2 vertices and a single edge
        return spanning_trees=1;

    if(size==3)
    {
        //Triangle?
        if( has_edge(1,0) && has_edge(2,1) && has_edge(2,0) )
            return 3;

        return spanning_trees=1;
    }

    auto matrix = new double[(size-1)*(size-1)];

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

    return spanning_trees=static_cast<uint64_t>(det*det + 0.5); //fast round(det*det)
}




OccurrenceCanonicizer::OccurrenceCanonicizer(unsigned int size) : size(size), words_needed(static_cast<size_t>(SETWORDSNEEDED(static_cast<int>(size))))
{
    g = new nauty_graph[size*words_needed];
    cang = new nauty_graph[size*words_needed];
    lab = new int[size];
    ptn = new int[size];
    orbits = new int[size];

    options.getcanon = MOTIVO_NAUTY_TRUE;
}

OccurrenceCanonicizer::~OccurrenceCanonicizer()
{
    delete[] g;
    delete[] cang;
    delete[] lab;
    delete[] ptn;
    delete[] orbits;

    nauty_freedyn();
    nautil_freedyn();
    naugraph_freedyn();
}

void OccurrenceCanonicizer::canonicize(Occurrence *occ)
{
    assert(size==occ->size);
#ifndef NEBUG
    nauty_check(MOTIVO_NAUTY_WORDSIZE, static_cast<int>(words_needed), static_cast<int>(size), NAUTYVERSIONID);
#endif

    EMPTYGRAPH(g, words_needed, size);

    for(unsigned int i=1; i<size; i++)
    {
        for (unsigned int j = 0; j < i; j++)
        {
            if (occ->has_edge(i, j))
                ADDONEEDGE (g, i, j, words_needed);
        }
    }

    densenauty(g, lab, ptn, orbits, &options, &stats, static_cast<int>(words_needed), static_cast<int>(size), cang);

    //From the nauty manual: the value of lab on return is the canonical labelling
    //of the graph. Precisely, it lists the vertices of g in the order in which they need to
    //be relabelled to give canong

    UndirectedGraph::vertex_t new_verts[16];
    memcpy(new_verts, occ->verts, sizeof(UndirectedGraph::vertex_t)*size);

    for(unsigned int i=0; i<size; i++)
        occ->verts[i] = new_verts[ lab[i] ];

    memset(occ->edges, 0, sizeof(uint8_t)*Occurrence::binary_footprint_bytes);
    for(unsigned int i=1; i<size; i++)
    {
        nauty_set* row = GRAPHROW(cang, i, words_needed);
        for(unsigned int j = 0; j < i; j++)
        {
            if( ISELEMENT( row, j) )
                occ->add_edge(i, j);
        }
    }

    occ->text_footprint_buffer[0]=0; //Invalidate text footprint
}
