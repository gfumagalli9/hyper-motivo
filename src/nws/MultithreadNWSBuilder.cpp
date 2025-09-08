#include "MultithreadNWSBuilder.h"
#include "../common/graph/Hypergraph.h"
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include <condition_variable>
#include "../builder/InclusionExclusionBuilder.h"

MultithreadNWSBuilder::MultithreadNWSBuilder(const Hypergraph* H, const TreeletList* treelet_list, const TreeletTable* treelet_table, std::ostream* output, unsigned int nthreads):
    H(H), 
    treelet_list(treelet_list), 
    treelet_table(treelet_table), 
    output(output), 
    nthreads(nthreads),
    builder(H, treelet_table){
    std::lock_guard lk(work_mutex);
    // Initialize work queue: singletons
    for (Hypergraph::edge_t he = 0; he < H->number_of_hyperedges(); he++){
        unsigned int size = H->hyperedge_size(he);
        std::vector<Hypergraph::vertex_t> he_verts;
        for (size_t i = 0; i < size; i++) he_verts.push_back(H->hyperedge_vertex(he, i));
        EdgeSubtype st = {{he}, he_verts, size};
        for(const auto &t : *treelet_list) {
            work_queue.push(std::make_pair(t, st));
            visited[t].insert(st);
            ++tasks_in_flight;      // conto un lavoro in più
        }
    }
};

void MultithreadNWSBuilder::build() {
    // 1) prepara lo stato di ciascun thread
    auto states  = new thread_state[nthreads];
    auto threads = new std::thread[nthreads];

    size_t N = H->number_of_vertices();
    size_t M = treelet_list->size();

    // 2) crea i worker
    for (unsigned int i = 0; i < nthreads; ++i) {
        // inizializza il vettore dei counts
        auto& m = states[i].nws_counts;
        m.reserve(treelet_list->size());
        for(const auto &t : *treelet_list) m.emplace(t, std::vector<int>(N, 0));
        // lancia il thread
        threads[i] = std::thread(&MultithreadNWSBuilder::worker_loop, this, i, states + i);
    }

    // 3) sveglia i worker che erano in attesa di work_condvar
    work_condvar.notify_all();

    // 4) aspetto che non ci siano più sottotipi "in volo"
    //    cioè che tasks_in_flight scenda a zero
    while (tasks_in_flight.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }

    // 5) tutti i lavori sono stati consumati: fermo i worker
    {
      std::lock_guard<std::mutex> lk(work_mutex);
      shutdown = true;
    }
    work_condvar.notify_all();  // sveglia eventuali worker bloccati

    // 6) attendo la terminazione di tutti i thread
    for (unsigned int i = 0; i < nthreads; ++i) {
        threads[i].join();
    }

    // 7) qui posso unire i risultati di states[i].nws_counts... ad esempio sommando tutte le counts in un unico vettore finale.
    std::unordered_map<Treelet, std::vector<int>, Treelet::TreeletHash> final_counts;

    for (unsigned th = 0; th < nthreads; ++th) {
        auto &nws_counts_t = states[th].nws_counts;  // è unordered_map<Treelet,vector<int>,...>
        for (auto &kv : nws_counts_t) {
            const Treelet &t            = kv.first;
            const std::vector<int> &src = kv.second;

            // crea il vettore se è la prima volta
            auto &dst = final_counts[t];      
            if (dst.empty()) dst.assign(N, 0);

            // somma elemento‐per‐elemento
            for (size_t v = 0; v < N; ++v) dst[v] += src[v];
        }
    }

    uint32_t nv = static_cast<uint32_t>(N);
    output->write(reinterpret_cast<const char*>(&nv), sizeof(nv));

    for (Hypergraph::vertex_t u = 0; u < N; ++u) {
        // 1) preparo la tabella (treelet, count) senza zeri
        std::vector<std::pair<Treelet,uint64_t>> tbl;
        tbl.reserve(final_counts.size());
        for (auto &kv : final_counts) {
            const Treelet &t = kv.first;
            const std::vector<int> &vec = kv.second;
            uint64_t c = static_cast<uint64_t>(vec[u]);
            if (c != 0) {
                tbl.emplace_back(t, c);
            }
        }
        // 2) serializzo u + tbl in un buffer già ordinato
        auto [buf, bytes] = builder.to_normalized_sorted_byte_array(u, tbl);
        // 3) lo scrivo su disco
        output->write(buf, static_cast<std::streamsize>(bytes));
        // 4) libero il buffer
        delete[] buf;
    }
    
    // 8) pulizia
    delete[] threads;
    delete[] states;
}

void MultithreadNWSBuilder::worker_loop(int thread_id, thread_state* state){
    while (true) {
        std::pair<Treelet,EdgeSubtype> work;
        { // preleva un lavoro o spegni
          std::unique_lock lk(work_mutex);
          work_condvar.wait(lk, [&] { return shutdown || !work_queue.empty(); });
          if (shutdown && work_queue.empty()) break;
          work = std::move(work_queue.front());
          work_queue.pop();
        }
        --tasks_in_flight;

        auto t = work.first;
        auto st = std::move(work.second);
        auto &counts = state->nws_counts[t];
    
        // Espandi “st” in parallelo:
        auto next = builder.build(st, t, counts);

        // Per ciascun nst, prova a inserirlo in visited e se OK mettilo in coda
        for (auto& st_next : next) { // Itero su (treelet, sottotipo)
            const Treelet &new_t  = t;
            EdgeSubtype  &new_st  = st_next;
            bool do_enqueue = false;
            {
                std::lock_guard vk(visited_mutex);
                auto [it, inserted] = visited[new_t].insert(new_st);
                do_enqueue = inserted;
            }
            if (do_enqueue) {
                {
                std::lock_guard lk(work_mutex);
                work_queue.push(std::move(std::make_pair(t, st_next)));
                }
                ++tasks_in_flight;    
                work_condvar.notify_one();
            }
        }
        
    }
};