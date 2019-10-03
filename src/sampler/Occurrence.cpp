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

#include "Occurrence.h"

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

Occurrence::Occurrence(const unsigned int size, const uint8_t* edges) {
	this->size = size;
	memcpy(this->edges, edges, binary_footprint_bytes);
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
        const unsigned int len = ((size*(size-1))/2  + 3)/4; //ceil( (size choose 2) / 4 ), i.e., one character every 4 entries in the adjacency matrix

        for(unsigned int i=0; i<len; i++)
        {
            if(i%2==0)
                text_footprint_buffer[i]= static_cast<char>('A'+ (edges[i/2]>>4u));
            else
                text_footprint_buffer[i]= static_cast<char>('A'+ (edges[i/2] & 0x0Fu));
        }
    }

    return text_footprint_buffer;
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

