// combine_counts.cpp
// Combina due tabelle .cnt (Low e High) sommando i conteggi per (Treelet, vertice)

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cassert>
#include "../common/treelets/TreeletTable.h"
#include "../common/io/CompressedRecordFile.h"
#include "../common/util.h"
#include "../common/OptionsParser.h"

// Struttura identica a merger.cpp
struct vertex_info {
    char* ptr;
    uint64_t count;
};

// Funzione per caricare una singola .cnt
void load_cnt(const std::string &fname, UndirectedGraph::vertex_t &num_vertices, std::vector<vertex_info> &info) {
    FILE *f = fopen(fname.c_str(), "rb");
    if (!f) throw std::runtime_error("Cannot open " + fname);
    UndirectedGraph::vertex_t nv;
    fread(&nv, sizeof(nv), 1, f);
    if (info.empty()) {
        num_vertices = nv;
        info.resize(nv);
    } else if (nv != num_vertices) {
        throw std::runtime_error("Vertex count mismatch in " + fname);
    }
    // mappa tutto in memoria
    fseeko(f, 0, SEEK_END);
    off_t sz = ftello(f);
    char* map_ptr = static_cast<char*>(motivo_mmap_populate(sz, PROT_READ, fileno(f)));
    char* ptr = map_ptr + sizeof(nv);
    char* end = map_ptr + sz;
    // leggi record per record
    while (ptr + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) <= end) {
        UndirectedGraph::vertex_t u;
        memcpy(&u, ptr, sizeof(u)); ptr += sizeof(u);
        uint64_t cnt;
        memcpy(&cnt, ptr, sizeof(cnt)); ptr += sizeof(cnt);
        assert(u < num_vertices);
        info[u].ptr   = ptr;
        info[u].count = cnt;
        ptr += cnt * sizeof(TreeletTable::treelet_count_pair);
    }
    if (ptr != end) throw std::runtime_error("Corrupted file " + fname);
    fclose(f);
}

// Funzione per scrivere la nuova .cnt
void write_cnt(const std::string &fname, UndirectedGraph::vertex_t num_vertices, const std::vector<std::vector<TreeletTable::treelet_count_pair>>& merged) {
    std::ofstream out(fname, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create " + fname);
    // header: numero di vertici
    out.write(reinterpret_cast<const char*>(&num_vertices), sizeof(num_vertices));
    // per ogni vertice
    for (UndirectedGraph::vertex_t u = 0; u < num_vertices; ++u) {
        auto &vec = merged[u];
        uint64_t cnt = vec.size();
        // scrivi id vertice + numero di occ.
        out.write(reinterpret_cast<const char*>(&u), sizeof(u));
        out.write(reinterpret_cast<const char*>(&cnt), sizeof(cnt));
        // build normalized, sorted array
        std::vector<TreeletTable::treelet_count_pair> norm(cnt);
        for (uint64_t i = 0; i < cnt; ++i) {
            norm[i].treelet = vec[i].treelet;
            norm[i].count = vec[i].count / norm[i].treelet.normalization_factor();
        }
        std::sort(norm.begin(), norm.end(), [](auto const &a, auto const &b){ return a.treelet < b.treelet; });
        // scrivi i (treelet, count) normalizzati
        out.write(reinterpret_cast<const char*>(norm.data()), cnt * sizeof(TreeletTable::treelet_count_pair));
    }
    out.close();
}

int main(int argc, char** argv) {
    OptionsParser op;
    // Define low and high input options
    auto low_opt = op.add_option(true, true, "low", '\0', "", "Low counts file (name-Low.cnt)");
    auto high_opt = op.add_option(true, true, "high", '\0', "", "High counts file (name-High.cnt)");
    auto help_opt = op.add_option(false, false, "help", 'h', "", "Print help and exit");
    auto output_opt = op.add_option(true, true, "output", 'o', "", "Output basename (without .cnt)");
    auto common_opt = op.add_option(false, true, "common", 'c', "", "Common counts file to subtract (without .cnt)");
    
    if (!op.parse(argc, const_cast<const char**>(argv)) || help_opt->is_found()) {
        std::cout << "Usage: low_high_merger --low low-file.cnt --high high-file.cnt -o output_basename [-c common_basename]\n";
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Retrieve and validate filenames
    std::string low_file = low_opt->get_value();
    std::string high_file = high_opt->get_value();
    std::string basename = output_opt->get_value();

    // carica entrambe le tabelle
    UndirectedGraph::vertex_t num_vertices = 0;
    std::vector<vertex_info> info_low, info_high;
    load_cnt(low_file, num_vertices, info_low);
    load_cnt(high_file, num_vertices, info_high);
    // prepara vettore per merged
    std::vector<std::vector<TreeletTable::treelet_count_pair>> merged(num_vertices);
    // per ogni vertice, leggi low+high e somma
    for (UndirectedGraph::vertex_t u = 0; u < num_vertices; ++u) {
        auto ptrL = info_low[u].ptr;
        auto ptrH = info_high[u].ptr;
        uint64_t cL = info_low[u].count;
        uint64_t cH = info_high[u].count;
        // carica i due array in vettori temporanei
        std::vector<TreeletTable::treelet_count_pair> vL(cL), vH(cH);
        memcpy(vL.data(), ptrL, cL * sizeof(vL[0]));
        memcpy(vH.data(), ptrH, cH * sizeof(vH[0]));
        // merge per treelet
        std::vector<TreeletTable::treelet_count_pair> tmp;
        tmp.reserve(cL + cH);
        size_t i = 0,j = 0;
        while (i < vL.size() && j < vH.size()) {
            if (vL[i].treelet < vH[j].treelet) tmp.push_back(vL[i++]);
            else if (vH[j].treelet < vL[i].treelet) tmp.push_back(vH[j++]);
            else {
                vL[i].count += vH[j].count;
                tmp.push_back(vL[i]);
                ++i; ++j;
            }
        }
        while (i < vL.size()) tmp.push_back(vL[i++]);
        while (j < vH.size()) tmp.push_back(vH[j++]);
        merged[u].swap(tmp);
    }
    // scrivi [name].cnt
    write_cnt(basename + ".cnt", num_vertices, merged);
    std::cout << "Combined written to " << basename << ".cnt\n";
    return EXIT_SUCCESS;
}