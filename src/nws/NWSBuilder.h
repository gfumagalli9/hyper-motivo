// Mantengo in questa classe la dinamica del singolo passaggio di NWS-> dato il sottotipo calcolo il suo peso e aggiorno count nws
#ifndef MOTIVO_NWS_BUILDER_H
#define MOTIVO_NWS_BUILDER_H

#include <vector>
#include "../common/graph/Hypergraph.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/Treelet.h"

typedef Hypergraph::vertex_t    Vertex;
typedef Hypergraph::edge_t      Edge;
typedef int64_t                 CountT;
typedef std::vector<Vertex>     VertexList;
typedef std::vector<Edge>       EdgeSubset;

class NWSBuilder{
private:
    const Hypergraph* H;
    const TreeletTable* nws_table;
public:
    NWSBuilder(const Hypergraph* H, const TreeletTable* nws_table):H(H), nws_table(nws_table){};

    /// @brief Extends subtype st and returns a vector containing all possible extensions of st, also update nws_counts. 
    /// Let v be a vertex in st (i.e - in the intersection of the hyperedges in st), an extension st' of st is defined as
    ///     st' = st \cup {e}, where e is an edge passing through v. 
    /// The returned vector contains all unique extensions of st.
    /// The procedure also updates counters of each vertex in st according to the inclusion-exclusion formula, 
    /// hence at the end of the procedure
    ///     nws_counts[v] = nws_counts[v] + (-1)^{size(st)}* weight(st) for each v
    /// @param st           Subtype to extend
    /// @param t            Treelet to retrieve the weight C(T, v) of each vertex
    /// @param nws_counts   Vector to be updated, contains weight for each vertex
    /// @return             Vector containing all possible extensions of st
    template<typename T> inline std::vector<EdgeSubtype> build [[gnu::hot]] (EdgeSubtype st, Treelet t, T& nws_counts){
        // Sign depends on st size
        uint16_t level = st.edges.size();
        CountT sign = (level & 1) ? +1 : -1;
        std::vector<EdgeSubtype> st_next;
    
        // Update nws_counts
        CountT sum = 0;
        for (Vertex x : st.verts) sum += nws_table->get_count(x, t);
        for (Vertex x : st.verts) {
            CountT  delta = sign * (sum - nws_table->get_count(x, t));
            nws_counts[x] += delta;
        }
        
        // Extends st: for each vertex u in st...
        for (Vertex u : st.verts) {
            // for each hyperedge passing through u...
            for (size_t j = 0; j < H->vertex_degree(u); ++j) {
                Edge he = H->incident_hyperedge(u, j);
                // ignore if he is already in st
                if (std::binary_search(st.edges.begin(), st.edges.end(), he)) continue;
                // otherwise he is a candidate to extend st
                EdgeSubset new_h = st.edges;
                new_h.insert(std::lower_bound(new_h.begin(), new_h.end(), he), he);
                // if the candidate is already in st, continue
                bool already = false;
                for (auto const &s : st_next) {
                    if (s.edges == new_h) {  
                        already = true;
                        break;
                    }
                }
                if(already) continue;
    
                // intersect he_verts and st.verts to compute new_h.verts
                VertexList he_verts;
                for (size_t i = 0; i < H->hyperedge_size(he); i++) he_verts.push_back(H->hyperedge_vertex(he, i));
                VertexList inters;
                inters.reserve(std::min(st.verts.size(), he_verts.size()));
                std::set_intersection(
                    st.verts.begin(), st.verts.end(),
                    he_verts.begin(), he_verts.end(),
                    std::back_inserter(inters)
                );
                if (inters.size() <= 1) continue;
                sum = 0;
                for (Hypergraph::vertex_t v : inters) sum += nws_table->get_count(v,t);
                st_next.push_back({new_h, inters, sum});
            }
        }
    
        return st_next;
    }

    template<typename T> inline std::pair<char*, std::size_t > to_normalized_sorted_byte_array [[gnu::hot]](const Hypergraph::vertex_t u, const T &table){
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
            counts[i].count = u_it->second;

            u_it++;
            i++;
        }

        std::sort(counts, counts+table.size(), [](const TreeletTable::treelet_count_pair& tc1, const TreeletTable::treelet_count_pair& tc2) { return tc1.treelet < tc2.treelet; } );

        return result;
    }
};



#endif
