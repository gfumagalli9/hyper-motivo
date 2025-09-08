#ifndef MOTIVO_IEBUILDER_H
#define MOTIVO_IEBUILDER_H

#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>
#include <set>

/// IEBuilder: costruisce i treelet di dimensione `size` tramite inclusione–esclusione
class IEBuilder {
private:
    const unsigned int size;
    const TreeletTableCollection* const lower;
    const TreeletTableCollection* const tIEc;
    const TreeletStructureSelector* const selector;
public:
    IEBuilder(
        unsigned int size,
        const TreeletTableCollection* lower,
        const TreeletTableCollection* tIEc,
        const TreeletStructureSelector* selector
    ) : size(size), lower(lower), tIEc(tIEc), selector(selector) {
        if (size == 0) {
            throw std::runtime_error("Invalid size for IEBuilder: must be ≥1");
        }
    }

    template<typename T> inline void combine [[gnu::hot]](Hypergraph::vertex_t v, T &counts) const {
        for (unsigned int size1 = 1; size1 < size; ++size1) {
            unsigned int size2 = size - size1;
            TreeletTable* t1_tab = lower->get_table(size1);
            TreeletTable* t2_tab = tIEc->get_table(size2);

            for (TreeletTable::const_iterator it1 = t1_tab->begin(v); !it1.is_over(); ++it1) {
                Treelet t1 = it1.treelet();
                TreeletTable::treelet_count_t c1 = it1.count();
                // La tabellina ie C(v, S, T) contiene la somma dei pesi dei vicini u di v dove i pesi sono C(u, S, T)
                for (TreeletTable::const_iterator it2 = t2_tab->begin(v); !it2.is_over(); ++it2) {
                    const Treelet t2 = it2.treelet();
                    assert(t2.is_valid());
                    assert(v_it.count() != 0);

                    Treelet merged = t1.merge(t2);
                    if(merged.is_valid() && (!selector || selector->is_included(merged.get_structure())))
                    {
                        TreeletTable::treelet_count_t &count = counts[merged];
                        TreeletTable::treelet_count_t tmp;
                        //printf("Calcolo C(T,S,v) per v %d = %d +  (%d * %d)\n", v, count, it1.count(), it2.count());
                        safe_mul(it1.count(), it2.count(), &tmp);
                        safe_add(count, tmp, &count);
                    }
                    else if(merged == invalid_merge_structure)
                        break; //All the remaining treelets t2 will have a structure that is too small.
                }
            }
        }
    }

    template<typename T> std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const Hypergraph::vertex_t u, const T &table, const bool normalize)
    {
        std::pair<char*, std::size_t> result;
        result.second = sizeof(Hypergraph::vertex_t) + sizeof(uint64_t) + table.size() * sizeof(TreeletTable::treelet_count_pair);
        result.first = new char[result.second];

        //Prevent alignment issues (size might get copied to unaligned memory)
        memcpy(result.first, &u, sizeof(Hypergraph::vertex_t));
        uint64_t size = table.size();
        memcpy(result.first + sizeof(Hypergraph::vertex_t), &size, sizeof(uint64_t));

        //Make sure array is properly aligned
        static_assert( (sizeof(Hypergraph::vertex_t) + sizeof(uint64_t)) % alignof(TreeletTable::treelet_count_pair) == 0, "treelet_count_pair not aligned in buffer" );
        auto counts = new(result.first+sizeof(Hypergraph::vertex_t)+sizeof(uint64_t)) TreeletTable::treelet_count_pair[size];
        TreeletTable::treelet_count_t i=0;
        typename T::const_iterator u_it = table.begin();
        while(u_it != table.end())
        {
            assert(u_it->second > 0);
            assert(u_it->second % u_it->first.normalization_factor() == 0);

            counts[i].treelet = u_it->first;
            if(normalize) counts[i].count = u_it->second / counts[i].treelet.normalization_factor();
            else counts[i].count = u_it->second;

            u_it++;
            i++;
        }

        std::sort(counts, counts+table.size(), [](const TreeletTable::treelet_count_pair& tc1, const TreeletTable::treelet_count_pair& tc2) { return tc1.treelet < tc2.treelet; } );

        return result;
    }
};

#endif // MOTIVO_IEBUILDER_H
