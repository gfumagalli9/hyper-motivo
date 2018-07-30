#ifndef MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H
#define MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H

#include <sparsehash/dense_hash_map>
#include "../platform/platform.h"
#include "../common/treelets/Treelet.h"

#include "../common/treelets/TreeletSelector.h"
#include "Occurrence.h"

class ColorCodingSpanningTreeCounter
{
private:
    //The number of spanning trees in a complete graph of 16 vertices is 16^14.
    //Considering overcounting we get values thar are <= 16^15 < 2^(15 log 16) = 2^60
	typedef google::dense_hash_map<Treelet, uint64_t , Treelet::TreeletHash> table_t;
#define COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(hm) do { (hm).set_empty_key(Treelet::invalid_treelet); } while(false)

    const Occurrence *occurrence;
    const TreeletSelector *selector;
    const unsigned int size;
    table_t **tables = nullptr;


    void do_build(unsigned int current_size);
    void combine(unsigned int u, unsigned int v, unsigned int current_size);

public:
    ColorCodingSpanningTreeCounter(const Occurrence* occurrence, TreeletSelector* selector=nullptr);

    ~ColorCodingSpanningTreeCounter();

    void count();
    uint64_t number_of_rooted_spanning_trees();
    uint64_t number_of_spanning_trees_rooted_at(unsigned int root);
};

#endif
