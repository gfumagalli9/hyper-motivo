// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "SpanningTreeCounter.h"

#include "ColorCodingSpanningTreeCounter.h"

#include "../common/util.h"
#include "../common/treelets/TreeletStructureSelector.h"

#define IDX(x,y) ( ((x)*((x)+1))/2 + (y) )
#define DIAG(x) ( (x)*((x)+3)/2 )

uint64_t SpanningTreeCounter::number_of_rooted_spanning_trees_kirchhoff(const Occurrence &occ)
{
    //if(spanning_trees!=0)
    //    return spanning_trees;

    const unsigned int size = occ.get_size();

    //Handle small cases
    if(size <= 3)
    {
        if(size==1)
            return 1; //Isolated vertex

        if(size == 2) //2 vertices and a single edge
            return 2;

        if (occ.has_edge(1, 0) && occ.has_edge(2, 1) && occ.has_edge(2, 0))
            return 9; //Triangle

        return 3; //Path on 3 vertices
    }

    //Positive definite 15x15 symmetric matrix stored compactly.
    static thread_local long double M[120];

    //Use Kirchhoff's method. Prepare M to contain a (size-1)x(size-1) submatrix of the Laplacian matrix of occ
    unsigned int nedges=0;
    for(unsigned int i=0; i<size-1; i++)
    {
        M[DIAG(i)] = 0;
        for(unsigned int j=0; j<i; j++)
        {
            if(occ.has_edge(i, j))
            {
                M[DIAG(i)]++; //Involves only elements already set to 0
                M[DIAG(j)]++; //Ditto
                M[IDX(i,j)]=-1;
                nedges++;
            }
            else
                M[IDX(i,j)]=0;
        }
    }

    //The degress of the Laplacian matrix are not accounting for the edges incident to the last vertex of the subgraph, add them
    for(unsigned int j=0; j<size-1; j++)
    {
        if(occ.has_edge(size - 1, j))
        {
            M[DIAG(j)]++;
            nedges++;
        }
    }

    if(nedges==size-1) //The subgraph is a tree
        return size;

    if(nedges==size*(size-1)/2) //Clique
        return ipow<uint64_t>(size, size-1); //size>4 here

    if(nedges==size*(size-1)/2-1) //Clique minus one edge
    {
        //Let TC = #Spanning trees in a clique of size vertices
        //Let E = #edges in a clique of size vertices = size * (size-1) / 2
        //Let Te = #spanning trees containing a fixed edge

        //Each spanning tree in a clique contains size-1 edges
        //By symmetry each edge is contained in the same number of spanning trees
        //I.e., TC = E * Te / (size-1)  =>  Te = TC*(size-1)/E

        //The number of spanning trees we are looking for is TC - Te
        // = TC * ( 1 - (size-1) / E )  =  TC * (E - (size-1)) / E
        // = TC * (size * (size-1) - 2(size-1) ) / (size * (size-1))
        // = TC * (size - 2) / size
        // = size^(size-3) * (size-2)

        //We multiply by size to account for the different rootings

        return ipow<uint64_t>(size, size-2) * (size-2); //size>4 here
    }

    //Compute LDL decomposition in-place
    //M stores both the input matrix, and the output matrix
    //D is a diagonal matrix and its diagonal is stored in the diagonal of M
    //L is a lower unit triangular matrix. Its lower triangular part is stored in the lower triangular part of M


    // Prints out the matrix for debug purposes
    /*
    for(unsigned int i=0; i<size-1; i++)
    {
        for(unsigned int j=0; j<i; j++)
            std::cerr << M[IDX(i,j)] << " ";
        std:: cerr << M[DIAG(i)] << "\n";
    }
    std::cerr << std::endl; */

    //Skip D[0][0] since it is already equal to M[0][0]
    for(unsigned int i=1; i<size; i++)
    {
        //L_ij = 1/D_jj( M_ij - sum_{k=0}{i-1} L_ik * D_kk * Ljk )
        for(unsigned int j = 0; j < i; j++)
        {
            for(unsigned int k = 0; k < j; k++)
                M[IDX(i, j)] -= M[IDX(i, k)] * M[DIAG(k)] * M[IDX(j, k)];

            M[IDX(i, j)] /= M[DIAG(j)];
        }

        //D_ii = M_ii - sum_{k=0}{i-1} L_ik^2 D_kk
        for(unsigned int k = 0; k < i; k++)
            M[IDX(i, i)] -= M[IDX(i, k)] * M[IDX(i, k)] * M[DIAG(k)];
    }

    //Compute determinant.
    //If we have a Cholesky decomposition M=C*C' then det(M) = \prod_i C_ii^2
    //In our case C = L sqrt(D), and hence C_ii = sqrt(D_ii) => det(M) = \prod_i D
    long double det = M[DIAG(0)];
    for(unsigned int i=1; i<size-1; i++)
        det*=M[DIAG(i)];

    return size*static_cast<uint64_t>(det+0.5); //fast round(det)
}

uint64_t SpanningTreeCounter::number_of_rooted_spanning_trees_colorcoding(const Occurrence &occ, const TreeletStructureSelector *ts)
{
    //FIXME: Figure out when its safe to only count the treelets rooted in 0?
    ColorCodingSpanningTreeCounter ccstc(&occ, false, ts);
    ccstc.count();
    return ccstc.number_of_counted_rooted_spanning_trees();
}

unsigned int SpanningTreeCounter::number_of_rooted_spanning_stars(const Occurrence &occ)
{
    unsigned int count = 0;
    for(unsigned int u=0; u<occ.get_size(); u++)
    {
        unsigned int deg=0;
        for(unsigned int v=0; v<u; v++)
            deg+=occ.has_edge(u,v);

        for(unsigned int v=u+1; v<occ.get_size(); v++)
            deg+=occ.has_edge(v,u);

        count += (deg == occ.get_size()-1)?1u:0u;
    }

    return count*occ.get_size();
}

SpanningTreeCounter::SpanningTreeCounter(const unsigned int size, const TreeletStructureSelector *selector) : size(size), selector(selector)
{
    if(size==0 || size>16)
        throw std::runtime_error("Invalid size");

    if(selector==nullptr)
    {
        strategy = STRATEGY_KIRCHOFF;
        return;
    }

    if(size>1 && !selector->is_included(Treelet::treelet_structure_highest_bit)) //one edge
    {
        strategy = STRATEGY_ZERO;
        return;
    }

    if(size<=2)
    {
        strategy = STRATEGY_KIRCHOFF;
        return;
    }

    Treelet::treelet_structure_t star_from_center = TreeletStructureSelector::star_from_center_structure(size);
    Treelet::treelet_structure_t star_from_leaf = TreeletStructureSelector::star_from_leaf_structure(size);

    bool only_stars;
    if(selector->get_mode() == TreeletStructureSelector::MODE_EXCLUDE)
    {
        //Check if the only excluded structures are stars of the given size
        only_stars = (selector->size()==2) && !selector->is_included(star_from_center) && !selector->is_included(star_from_leaf);
    }
    else
    {
        TreeletStructureSelector star_selector(TreeletStructureSelector::MODE_INCLUDE);
        star_selector.add_structure_with_current_mode(star_from_center);
        star_selector.add_structure_with_current_mode(star_from_leaf);
        star_selector = star_selector.buildable_closure();

        only_stars = true;
        for(auto it = selector->begin(); only_stars && it!=selector->end(); it++)
            if(Treelet::number_of_vertices(*it)==size)
                only_stars = (*it == star_from_center || *it==star_from_leaf);

        for(auto it = star_selector.begin(); only_stars && it!=star_selector.end(); it++)
            only_stars = selector->is_included(*it);
    }


    if(only_stars)
    {
        if(selector->get_mode() == TreeletStructureSelector::MODE_INCLUDE)
            strategy=STRATEGY_STARS;
        else
            strategy=STRATEGY_KIRCHOFF_MINUS_STARS;

        return;
    }

    strategy=STRATEGY_COLOR_CODING;
}


uint64_t SpanningTreeCounter::number_of_rooted_spanning_trees(const Occurrence &occ)
{
    switch(strategy)
    {
        case STRATEGY_KIRCHOFF:
            return number_of_rooted_spanning_trees_kirchhoff(occ);
        case STRATEGY_KIRCHOFF_MINUS_STARS:
            return number_of_rooted_spanning_trees_kirchhoff(occ) - number_of_rooted_spanning_stars(occ);
        case STRATEGY_STARS:
            return number_of_rooted_spanning_stars(occ);
        case STRATEGY_COLOR_CODING:
            return number_of_rooted_spanning_trees_colorcoding(occ, selector);
        case STRATEGY_ZERO:
        default:
            return 0;
    }
}