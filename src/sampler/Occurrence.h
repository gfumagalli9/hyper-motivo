//
// Created by steven on 12/18/16.
//

#ifndef MOTIVO_OCCURRENCE_H
#define MOTIVO_OCCURRENCE_H


#include "../common/UndirectedGraph.h"
#include "../common/Treelet.h"

class Occurrence
{
private:
    const unsigned int size;
    UndirectedGraph::vertex_t vertices[16];
    uint8_t edges[15] = {0}; //edge (i,j) with i>j is in position sum_{k=1}^(i-1) k + j = (i-1)*i/2 + j

    inline void add_edge(unsigned int i, unsigned int j)
    {
        assert(i>j);
        unsigned int pos = (i-1)*i/2 + j;
        edges[pos/8] |= static_cast<uint8_t>(0b10000000 >> (pos%8));
    }

    inline bool has_edge(unsigned int i, unsigned int j)
    {
        assert(i>j);
        unsigned int pos = (i-1)*i/2 + j;
        return ( edges[pos/8] & (0b10000000 >> (pos%8)) ) != 0;
    }

public:
    Occurrence(const UndirectedGraph::vertex_t* occ, const Treelet& treelet);
    Occurrence(const unsigned int size, const UndirectedGraph::vertex_t* occ, const UndirectedGraph* graph);

    void canonicize();

    std::string footprint();

    std::string to_string();
};


#endif //MOTIVO_OCCURRENCE_H
