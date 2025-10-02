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
#include "../common/io/PropertyStore.h"
#include <filesystem>



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
void write_cnt(const std::string &fname,
               UndirectedGraph::vertex_t num_vertices,
               std::vector<std::vector<TreeletTable::treelet_count_pair>>& merged)
{
    std::ofstream out(fname, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create " + fname);

    // Write header: number of vertices
    out.write(reinterpret_cast<const char*>(&num_vertices), sizeof(num_vertices));

    // IMPORTANT: 'merged[u]' is already sorted by 'treelet' thanks to the merge step.
    // We normalize counts *in place* and write directly, avoiding extra allocations
    // and the redundant final sort.
    for (UndirectedGraph::vertex_t u = 0; u < num_vertices; ++u) {
        auto &vec = merged[u];
        const uint64_t cnt = static_cast<uint64_t>(vec.size());

        // write vertex id + number of records
        out.write(reinterpret_cast<const char*>(&u), sizeof(u));
        out.write(reinterpret_cast<const char*>(&cnt), sizeof(cnt));

        // In-place normalization: divide counts by the treelet's normalization factor.
        // This preserves the order by 'treelet', so no re-sort is required.
        for (auto &p : vec) {
            const uint64_t nf = p.treelet.normalization_factor();
            if (nf > 1) p.count /= nf; // avoid an unnecessary division when nf==1
        }

        // Write normalized pairs
        if (cnt) {
            out.write(reinterpret_cast<const char*>(vec.data()),
                      static_cast<std::streamsize>(cnt * sizeof(TreeletTable::treelet_count_pair)));
        }
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
        const uint64_t cL = info_low[u].count;
        const uint64_t cH = info_high[u].count;

        // Load both arrays into aligned vectors (keeps the current safe approach).
        std::vector<TreeletTable::treelet_count_pair> vL, vH;
        vL.resize(cL);
        vH.resize(cH);
        if (cL) std::memcpy(vL.data(), ptrL, cL * sizeof(vL[0]));
        if (cH) std::memcpy(vH.data(), ptrH, cH * sizeof(vH[0]));

        // Fast paths when one side is empty (keeps order, avoids merge loop).
        auto &outVec = merged[u];
        if (cL == 0) {
            outVec = std::move(vH);
            continue;
        }
        if (cH == 0) {
            outVec = std::move(vL);
            continue;
        }

        // Classic two-way merge directly into merged[u] (already sorted inputs).
        outVec.clear();
        outVec.reserve(static_cast<size_t>(cL + cH));

        size_t i = 0, j = 0;
        while (i < vL.size() && j < vH.size()) {
            const auto &a = vL[i];
            const auto &b = vH[j];
            if (a.treelet < b.treelet) {
                outVec.push_back(a);
                ++i;
            } else if (b.treelet < a.treelet) {
                outVec.push_back(b);
                ++j;
            } else {
                // same treelet: sum counts
                TreeletTable::treelet_count_pair s = a;
                s.count += b.count;
                outVec.push_back(s);
                ++i; ++j;
            }
        }
        // Flush the tail
        while (i < vL.size()) outVec.push_back(vL[i++]);
        while (j < vH.size()) outVec.push_back(vH[j++]);
    }
    // scrivi [name].cnt
    write_cnt(basename + ".cnt", num_vertices, merged);

    const std::string info_low_path  = std::filesystem::path(low_file ).replace_extension(".info").string();
    const std::string info_high_path = std::filesystem::path(high_file).replace_extension(".info").string();

    PropertyStore properties_low(info_low_path);
    PropertyStore properties_high(info_high_path);
    const bool storeonly0_low  = properties_low .get_bool("StoreOnlyOn0", false);
    const bool storeonly0_high = properties_high.get_bool("StoreOnlyOn0", false);

    // Costruisci il path DI OUTPUT per l'.info del file combinato.
    // 'basename' è già “output basename (without .cnt)”, quindi basta aggiungere ".info".
    const std::string info_out = basename + ".info";

    if (storeonly0_low == storeonly0_high) {
        PropertyStore properties_out;
        properties_out.set_bool("StoreOnlyOn0", storeonly0_low); // preserva il valore coerente (true/false)
        properties_out.save(info_out);                           // <-- qui il punto è fondamentale
    } else {
        throw std::runtime_error(
            std::string("StoreOnlyOn0 mismatch between low/high: low=") +
            (storeonly0_low ? "true" : "false") + ", high=" +
            (storeonly0_high ? "true" : "false"));
    }

    std::cout << "Combined written to " << basename << ".cnt\n";
    return EXIT_SUCCESS;
}