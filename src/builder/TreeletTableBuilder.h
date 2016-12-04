//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_TREELETTABLEBUILDER_H
#define MOTIVO_TREELETTABLEBUILDER_H

#include <sparsehash/sparse_hash_map>
#include <string>
#include <cmph.h>
#include <functional>
#include "../Treelet.h"
#include "../UndirectedGraph.h"
#include "../GraphColoring.h"
#include "../TreeletTable.h"
#include "../TreeletTableCollection.h"

class TreeletTableBuilder
{
private:
    struct TreeletHash
    {
        inline size_t operator() (const Treelet t) const
        {
            return t.hash();
        }
    };

    typedef google::sparse_hash_map<Treelet, TreeletTable::treelet_count_t, TreeletHash> table_t;

    const UndirectedGraph* graph;
    const UndirectedGraph::vertex_t num_vertices;
    const GraphColoring* coloring;
    const unsigned int size;
    const TreeletTableCollection* lower;

    table_t** counts;

    /// Fills a size-1 table
    void do_fill_1();

    /// Fills a table for sizes > 1
    void do_fill();

    /// Combines the treelets of vertex @param u with the treelets of vertex @param v
    void combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v);

    /// Normalizes the counts of treelets rooted in @param u
    void normalize(long u);

    //cmph adapter structure and methods to compute a minimum perfect hash function directly from the sparse hash map
    struct sparsehash_data_t
    {
        const table_t::const_iterator begin;
        table_t::const_iterator current;
        const table_t::const_iterator end;

        sparsehash_data_t(const table_t* table) : begin(table->begin()), current(begin), end(table->end()) {};
    };

    static cmph_io_adapter_t* sparsehash_adapter(const table_t* table);
    static int key_sparsehash_read(void *data, char **key, cmph_uint32 *keylen);
    static void key_sparsehash_dispose(void *data, char *key, cmph_uint32 keylen);
    static void key_sparsehash_rewind(void *data);

public:

    TreeletTableBuilder(UndirectedGraph* graph, GraphColoring* coloring, const unsigned int size, const TreeletTableCollection* lower = NULL);
    ~TreeletTableBuilder();

    /// Fills the treelet table computing the number of treelets of each kind rooted at each vertex
    void build();

    /// Utility function for writing the table contents using the given @param basename
    /// This call is equivalent to calling write_data and, if size>1, write_phf
    void write(const std::string& basename) const;

    ///Writes the content of the table to the given @basename
    ///Two files will be created. The first is basename.dat containing the raw key-value pairs,
    ///the second is basename.off containing, for each vertex, the associated offset in the .dat file
    void write_data(const std::string &basename) const;

    ///Computes and writes a set of perfect hash functions (one for each vertex) to aid retrieval
    ///The functions are written to a fine named @param basename.phf
    ///@returns the number of perfect hash function that were successfully computed
    long write_phf(const std::string &basename) const;

};


#endif //MOTIVO_TREELETTABLEBUILDER_H
