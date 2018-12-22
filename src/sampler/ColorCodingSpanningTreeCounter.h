#ifndef MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H
#define MOTIVO_COLORCODINGSPANNINGTREECOUNTER_H

#include "Occurrence.h"
#include <sparsehash/dense_hash_map>
#include "../platform/platform.h"
#include "../common/treelets/Treelet.h"

#include "../common/treelets/TreeletSelector.h"
#include "CachedSTC.h"

class ColorCodingSpanningTreeCounter
{
public:
	typedef google::dense_hash_map<Treelet, uint64_t , Treelet::TreeletHash> table_t;
private:
    //The number of spanning trees in a complete graph of 16 vertices is 16^14.
    //Considering overcounting we get values thar are <= 16^15 < 2^(15 log 16) = 2^60
#define COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(hm) do { (hm).set_empty_key(Treelet::invalid_treelet); } while(false)

    const Occurrence *occurrence;
    const TreeletSelector *selector;
    const unsigned int size;
    table_t **tables = nullptr;


    void do_build(unsigned int current_size);
    void combine(unsigned int u, unsigned int v, unsigned int current_size);

public:
    ColorCodingSpanningTreeCounter(const Occurrence* occurrence, const TreeletSelector* selector=nullptr);

    ~ColorCodingSpanningTreeCounter();

    void count();
    uint64_t number_of_rooted_spanning_trees();
    uint64_t number_of_spanning_trees_rooted_at(unsigned int root);
    uint64_t number_of_spanning_trees();

    // get the spanning tree count table for a given root node
    inline const table_t &get_table(int root) {
    	return tables[size-1][root];
    }

    /* FIXME: What is going on here?
    // get the global spanning tree count table (sum over all nodes)
    inline const table_t get_table() {
    	table_t result; // = new table_t();
    	COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(result);
    	for (int u = 0; u < size; u++)
    		for (auto &it : tables[size-1][u])
    			result[it.first] += it.second;
    	return result;
    }
*/
    // get the global spanning tree count table (sum over all nodes)
    inline void get_table(CachedSTC::treelet_table_t* tab) {
    	for (unsigned int u = 0; u < size; u++)
    		for (auto &it : tables[size-1][u])
    			(*tab)[it.first] += it.second;
    }
};

#endif
