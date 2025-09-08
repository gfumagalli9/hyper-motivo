#include "SequentialNWSBuilder.h"
#include <cstring>      // for memcpy
#include <stdexcept>
#include <algorithm>
#include <chrono>       // for timing debug
#include "../builder/InclusionExclusionBuilder.h"

SequentialNWSBuilder::SequentialNWSBuilder( const Hypergraph* H, TreeletList* treelet_list, TreeletTable* treelet_table, std::ostream* output) noexcept
    : H(H), treelet_list(treelet_list), treelet_table(treelet_table), output(output), builder(H, treelet_table){ 
    nws_counts.reserve(treelet_list->size());
    for(const auto &t : *treelet_list) nws_counts.emplace(t, std::vector<int>(H->number_of_vertices(), 0));    
}

void SequentialNWSBuilder::build() {
    if (!H || !treelet_list || !treelet_table || !output) 
        throw std::runtime_error("SequentialNWSBuilder: puntatori invalidi");

    // 1) Header: numero di vertici
    uint32_t nv = static_cast<uint32_t>(H->number_of_vertices());
    output->write(reinterpret_cast<const char*>(&nv), sizeof(nv));

    // Singletons
    for (Edge he = 0; he < H->number_of_hyperedges(); ++he) {
        unsigned int size = H->hyperedge_size(he);
        std::vector<Hypergraph::vertex_t> he_verts;
        for (size_t i = 0; i < size; i++) he_verts.push_back(H->hyperedge_vertex(he, i));
        EdgeSubtype st = {{he}, he_verts, size};
        for(const auto &t : *treelet_list) {
            work_queue.push(std::make_pair(t, st));
            visited[t].insert(st);
        }
    }

    // 2) Pre‐calcolo: per ogni treelet ti, calcola inclusion–exclusion NWS
    while (!work_queue.empty()){
        std::pair<Treelet,EdgeSubtype> work;
        work = std::move(work_queue.front());
        work_queue.pop();
        Treelet cur_treelet = work.first;
        EdgeSubtype cur_st = work.second;
        auto next = builder.build(cur_st, cur_treelet, nws_counts[cur_treelet]);
        for (auto& st_next : next) {
            const Treelet &new_t  = cur_treelet;
            EdgeSubtype  &new_st  = st_next;
            bool do_enqueue = false;
            auto [it, inserted] = visited[new_t].insert(new_st);
            do_enqueue = inserted;
            if (do_enqueue) 
                work_queue.push(std::move(std::make_pair(cur_treelet, st_next)));
        }
    }

    // 3) Scrivi record per ciascun vertice
    for (Vertex u = 0; u < H->number_of_vertices(); ++u) {
        std::vector<std::pair<Treelet,uint64_t>> tbl;
        tbl.reserve(nws_counts.size());
        for (auto &kv : nws_counts) {
            const Treelet &t = kv.first;
            const std::vector<int> &vec = kv.second;
            uint64_t c = static_cast<uint64_t>(vec[u]);
            if (c != 0) {
                tbl.emplace_back(t, c);
            }
        }
        auto [buf, bytes] = builder.to_normalized_sorted_byte_array(u, tbl);
        output->write(buf, static_cast<std::streamsize>(bytes));
        delete[] buf;
    }
}
