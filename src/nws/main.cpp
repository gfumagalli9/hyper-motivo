// motivo-nws.cpp
// MIT License
// … (intestazione come negli altri tool) …

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletList.h"
#include "../common/io/CompressedRecordFile.h"
#include "../common/io/PropertyStore.h"
#include "SequentialNWSBuilder.h"
#include "MultithreadNWSBuilder.h"


struct opts_t {
    std::string graph_file;
    unsigned int size;
    std::string tables_basename;
    std::string output_basename;
    unsigned int threads;
    bool        allow_singletons;
};

bool parse_args(int argc, const char** argv, opts_t &opts) {
    OptionsParser op;
    auto *help  = op.add_option(false,false,"help",'h',"","Print help and exit");
    auto *g     = op.add_option(true, true, "graph", 'g', "", "Input hypergraph basename");
    auto *s     = op.add_option(true, true, "size",  's', "", "Treelet size (hypergraph IE)");
    auto *i     = op.add_option(true, true, "input", 'i', "", "Basename of count files (\"tables_basename.size.cnt\" and \".treelets.dtz\")");
    auto *o     = op.add_option(true, true, "output",'o', "", "Output basename");
    auto *t     = op.add_option(false, true, "threads", 't', "1", "Number of threads to use");
    auto *no_prune = op.add_option(false, false, "no-subtype-pruning", '\0', "","Disable NWS early stop on singleton intersections (keep |S'|=1 subtypes)");
    
    if (!op.parse(argc, argv) || help->is_found()) {
        std::cout << "motivo-nws [OPTION]...\n" << op.help();
        return false;
    }
    opts.graph_file      = g->get_value();
    opts.size            = std::stoul(s->get_value());
    opts.tables_basename = i->get_value();
    opts.output_basename = o->get_value();
    opts.threads = std::stoul(t->get_value());
    opts.allow_singletons = no_prune->is_found();
    return true;
}

int main(int argc, const char** argv) {
    opts_t opts;
    if (!parse_args(argc, argv, opts)) return EXIT_SUCCESS;

    std::cout << "Using " << opts.threads << " thread(s)\n";

    std::cout << "This is motivo-nws (hypergraph IE). Size=" << opts.size
              << "  Graph=\"" << opts.graph_file << "\"  Base=\""
              << opts.tables_basename << "\"\n";

    // 1) Carico l'ipergrafo
    Hypergraph H(opts.graph_file);
    H.prefault();
    auto num_vertices = H.number_of_vertices();
    std::cout << "Hypergraph: |V|=" << num_vertices
              << "  |E|=" << H.number_of_hyperedges() << "\n";

    // 2) Costruisco il reader per il TreeletTable (conteggi base) e carico in Collection
    TreeletTableCollection ttc;
    std::string cnt_file = opts.tables_basename + "." + std::to_string(opts.size) + ".dtz";
    CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias> reader_cnt;
    reader_cnt.open(cnt_file);
    reader_cnt.prefault(0, num_vertices-1);

    TreeletTable baseTable(&reader_cnt);
    ttc.add(&baseTable);

    // 3) Costruisco il reader per la lista di treelet unici e la TreeletList
    std::string treelets_file = opts.tables_basename + "." + std::to_string(opts.size) + ".treelets.dtz";
    CompressedRecordFileReader<const Treelet, /*RAW=*/false> reader_tl;
    reader_tl.open(treelets_file);
    reader_tl.prefault(0, /*num_records=*/reader_tl.number_of_records()-1);

    TreeletList tl(&reader_tl);

    // 4) Preparo lo stream di output ".ie.cnt"
    std::string ie_file = opts.output_basename + "." + std::to_string(opts.size) + ".ie.cnt";
    std::ofstream out(ie_file, std::ofstream::binary | std::ofstream::trunc);
    if (!out.good()) {
        std::cerr << "Error: cannot open " << ie_file << "\n";
        return EXIT_FAILURE;
    }

    // 5) Eseguo il builder NWS (Sequential o Multithread)
    if(opts.threads <= 1) {
        SequentialNWSBuilder builder(&H, &tl, &baseTable, &out, opts.allow_singletons);
        builder.build();
    } else {
        MultithreadNWSBuilder builder(&H, &tl, &baseTable, &out, opts.threads, opts.allow_singletons);
        builder.build();
    }

    out.close();
    std::cout << "IE table written to " << ie_file << "\n";

    return EXIT_SUCCESS;
}