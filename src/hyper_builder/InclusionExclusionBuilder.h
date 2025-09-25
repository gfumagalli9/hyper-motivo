#ifndef MOTIVO_IEBUILDER_H
#define MOTIVO_IEBUILDER_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "InclusionExclusionBuilder.h"              // safe_add/safe_mul, invalid_merge_structure
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"

/// IEBuilder
/// ------------
/// Given:
///   - target size `k`
///   - `lower`  = DP tables C_i(v) for i = 1..k-1 (GLOBAL, fused LOW+HIGH)
///   - `tIEc`   = IE helper tables IE_j(v) for j = 1..k-1 (built on HIGH)
///   - optional `selector` to filter structures
///
/// For a fixed root vertex v, `combine(v, counts)` computes all size-k treelet
/// counts by merging every pair (t1 in C_i(v)) × (t2 in IE_{k-i}(v)),
/// for i = 1..k-1, using Treelet::merge(). Each valid merge contributes
/// it1.count() * it2.count() to the resulting size-k treelet keyed by `merged`.
///
/// The result is accumulated into `counts`, a map-like container
///   Key   = Treelet
///   Value = TreeletTable::treelet_count_t
///
/// Finally `to_normalized_sorted_byte_array()` serializes the per-vertex map
/// into the on-disk record layout used by Treelet tables:
///   [vertex (u)][#pairs][(Treelet,Count) ...] (pairs sorted by Treelet)
class IEBuilder {
private:
    const unsigned int                       size;     // target k (>=1)
    const TreeletTableCollection*     const  lower;    // C_i,  i=1..k-1
    const TreeletTableCollection*     const  tIEc;     // IE_j, j=1..k-1
    const TreeletStructureSelector*   const  selector; // optional filter

public:
    IEBuilder(unsigned int size,
              const TreeletTableCollection* lower,
              const TreeletTableCollection* tIEc,
              const TreeletStructureSelector* selector)
        : size(size), lower(lower), tIEc(tIEc), selector(selector)
    {
        if (size == 0) {
            throw std::runtime_error("Invalid size for IEBuilder: must be >= 1");
        }
    }

    /// Combine all partitions size = size1 + size2, 1 <= size1 < size.
    /// For each root v, this builds the size-`size` table row by merging
    /// entries from lower->get_table(size1) and tIEc->get_table(size2)
    /// rooted at v. Valid merges are accumulated into `counts`.
    ///
    /// `T` must be a map-like container with:
    ///   - `T::mapped_type` convertible to TreeletTable::treelet_count_t
    ///   - operator[](const Treelet&) returning a reference to the count
    template<typename T>
    inline void combine [[gnu::hot]](Hypergraph::vertex_t v, T& counts) const
    {
        for (unsigned int size1 = 1; size1 < size; ++size1) {
            const unsigned int size2 = size - size1;

            TreeletTable* t1_tab = lower->get_table(size1);
            TreeletTable* t2_tab = tIEc->get_table(size2);

            for (TreeletTable::const_iterator it1 = t1_tab->begin(v); !it1.is_over(); ++it1) {
                const Treelet t1 = it1.treelet();
                // it1.count() is the cumulative-difference count for t1 at v
                assert(t1.is_valid());
                assert(it1.count() != 0);

                for (TreeletTable::const_iterator it2 = t2_tab->begin(v); !it2.is_over(); ++it2) {
                    const Treelet t2 = it2.treelet();
                    assert(t2.is_valid());
                    assert(it2.count() != 0);

                    const Treelet merged = t1.merge(t2);

                    if (merged.is_valid() &&
                        (!selector || selector->is_included(merged.get_structure())))
                    {
                        // counts[merged] += it1.count() * it2.count()  (overflow-safe)
                        TreeletTable::treelet_count_t& dst = counts[merged];
                        TreeletTable::treelet_count_t tmp;
                        safe_mul(it1.count(), it2.count(), &tmp);
                        safe_add(dst, tmp, &dst);
                    }
                    // Optimization: if merge reports the sentinel "too small" structure,
                    // the remaining t2 (in iteration order) will not help—stop early.
                    else if (merged == invalid_merge_structure) {
                        break;
                    }
                }
            }
        }
    }

    /// Serialize a per-vertex table into the on-disk record used by TreeletTable.
    /// Layout:
    ///   [u: Hypergraph::vertex_t]
    ///   [n: uint64_t]                      <- number of pairs
    ///   n × (Treelet, count)               <- packed, sorted by Treelet
    ///
    /// If `normalize` is true, each count is divided by
    ///   Treelet::normalization_factor() before writing.
    template<typename T>
    std::pair<char*, std::size_t>
    to_normalized_sorted_byte_array [[gnu::hot]](const Hypergraph::vertex_t u,
                                                 const T& table,
                                                 const bool normalize)
    {
        using Pair = TreeletTable::treelet_count_pair;

        std::pair<char*, std::size_t> out;
        const uint64_t n = static_cast<uint64_t>(table.size());
        out.second = sizeof(Hypergraph::vertex_t) + sizeof(uint64_t) + n * sizeof(Pair);
        out.first  = new char[out.second];

        // Write header [u][n]
        std::memcpy(out.first, &u, sizeof(Hypergraph::vertex_t));
        std::memcpy(out.first + sizeof(Hypergraph::vertex_t), &n, sizeof(uint64_t));

        // Ensure proper alignment of the array that follows
        static_assert((sizeof(Hypergraph::vertex_t) + sizeof(uint64_t)) % alignof(Pair) == 0,
                      "treelet_count_pair not aligned in buffer");
        auto* pairs = new (out.first + sizeof(Hypergraph::vertex_t) + sizeof(uint64_t)) Pair[n];

        // Copy (Treelet, Count), possibly normalized
        TreeletTable::treelet_count_t i = 0;
        for (auto it = table.begin(); it != table.end(); ++it, ++i) {
            const Treelet& t = it->first;
            auto c = it->second;

            assert(c > 0);
            assert(t.is_valid());
            assert(c % t.normalization_factor() == 0);

            pairs[i].treelet = t;
            pairs[i].count   = normalize ? (c / t.normalization_factor()) : c;
        }

        // Sort pairs by Treelet to match the on-disk invariant
        std::sort(pairs, pairs + n,
                  [](const Pair& a, const Pair& b) { return a.treelet < b.treelet; });

        return out;
    }
};

#endif // MOTIVO_IEBUILDER_H