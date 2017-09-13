//
// Created by steven on 12/18/16.
//

#ifndef MOTIVO_OCCURRENCE_H
#define MOTIVO_OCCURRENCE_H


#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "include_nauty.h"

class OccurrenceCanonicizer;

class Occurrence
{
friend class OccurrenceCanonicizer;

public:
    const unsigned int size;

    //i,j in {0,...,15}
    //edge (i,j) with i>j is in position sum_{k=1}^(i-1) k + j = (i-1)*i/2 + j in edges
    //last bit is the one corresponding to i=15, j=14 => at most 119 bits are need (14 bytes, 7 bits)
    constexpr static unsigned int binary_footprint_bits = 119;
    constexpr static unsigned int binary_footprint_bytes = (binary_footprint_bits+7)/8; //round up (to 15 bytes)
    constexpr static unsigned int text_footprint_bytes = binary_footprint_bytes*2;

private:
    UndirectedGraph::vertex_t verts[16] = {0};
    uint8_t edges[binary_footprint_bytes] = {0};
    uint64_t spanning_trees = 0;

    char text_footprint_buffer[text_footprint_bytes+1] = {0}; //Add null-terminator

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
    Occurrence() : size(0) {}; //Empty constructor to take advantage of Stack allocation
    Occurrence(const Treelet& treelet, const UndirectedGraph::vertex_t* occ);
    Occurrence(const unsigned int size, const UndirectedGraph* graph, const UndirectedGraph::vertex_t* occ);

    ///@returns the number of spanning trees of this occurrence
    uint64_t number_of_spanning_trees();

    const UndirectedGraph::vertex_t* vertices() const { return verts; };
    const char* binary_footprint() const { return reinterpret_cast<const char*>(edges); };

    const char* text_footprint();
};


//The underlying library used to canonicize the occurrence requires initialization and cleanup to be used
//from multiple threads. We use this friend class to save on this overhead.
//A single instance of this class is not thread safe. However distinct instances can be used by differenc threads.
class OccurrenceCanonicizer
{
private:
    const int size;
    const size_t words_needed;

    nauty_graph* g;
    nauty_graph *cang;
    int *lab;
    int *ptn;
    int *orbits;

    DEFAULTOPTIONS_GRAPH(options);
    statsblk stats;

public:
    OccurrenceCanonicizer(unsigned int size);
    ~OccurrenceCanonicizer();

    void canonicize(Occurrence* occ);
};

#endif //MOTIVO_OCCURRENCE_H
