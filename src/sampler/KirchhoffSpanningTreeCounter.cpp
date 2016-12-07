//
// Created by steven on 11/29/16.
//

#include "KirchhoffSpanningTreeCounter.h"
#include <lapacke.h>

KirchhoffSpanningTreeCounter::KirchhoffSpanningTreeCounter(const UndirectedGraph* graph, const unsigned int size) : graph(graph), size(size)
{
    assert(graph!=nullptr);
    assert(size>0);

    if(size>=4)
        matrix = new double[(size-1)*(size-1)];
    else
        matrix = nullptr;
}

KirchhoffSpanningTreeCounter::~KirchhoffSpanningTreeCounter()
{
    if(matrix!=nullptr)
        delete[] matrix;
}

uint64_t KirchhoffSpanningTreeCounter::count(const UndirectedGraph::vertex_t* subgraph)
{
    //Handle small cases
    if(size<=2) //Isolated vertex or 2 vertices and a single edge
        return 1;

    if(size==3)
    {
        //Triangle?
        if( graph->has_edge(subgraph[0], subgraph[1]) && graph->has_edge(subgraph[1], subgraph[2]) && graph->has_edge(subgraph[2], subgraph[0]) )
            return 3;

        return  1;
    }

    //Compute the num_vertices-1 x num_vertices-1 submatrix of the Laplacian matrix of the subgraph of G induced by "subgraph"
    unsigned int nedges=0;
    for(unsigned int i=0; i<size-1; i++)
    {
        matrix[i*size] = 0; //i*num_vertices == i*(num_vertices-1)+i
        for(unsigned int j=0; j<i; j++)
        {
            if(graph->has_edge(subgraph[i], subgraph[j]))
            {
                matrix[i*size]++; //Involves only elements already set to 0
                matrix[j*size]++; //Ditto
                matrix[i*(size-1)+j]=-1;
                nedges++;
            }
            else
                matrix[i*(size-1)+j]=0;

            //nedges+=matrix[i*num_vertices]; //matrix[i*num_vertices] == # of edges incident to i such that the other endpoint is smaller than i
        }
    }

    //The degress of the Laplacian matrix are not accounting  for the edges incident to the last vertex of the subgraph, add them
    for(unsigned int j=0; j<size-1; j++)
    {
        if(graph->has_edge(subgraph[size - 1], subgraph[j]))
        {
            matrix[j*size]++;
            nedges++;
        }
    }

    if(nedges==size-1) //The subgraph is a tree
        return 1;

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

    return static_cast<uint64_t>(det*det + 0.5); //fast round(det*det)

}

