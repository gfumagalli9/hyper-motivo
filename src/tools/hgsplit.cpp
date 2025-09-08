// MIT License
//
// Hypergraph split tool: divide an input hypergraph into two based on edge-size threshold
// and compute common neighbor pairs between small and large parts.

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/io/PropertyStore.h"

using vertex_t = Hypergraph::vertex_t;
using edge_t   = Hypergraph::edge_t;

static void write_hypergraph(const std::string &basename, const std::vector<std::vector<vertex_t>> &hyperedges, const vertex_t num_verts) {
    edge_t   num_edges = static_cast<edge_t>(hyperedges.size());

    // build invert mapping
    std::vector<std::vector<edge_t>> v2e(num_verts);
    for (edge_t e = 0; e < num_edges; ++e)
        for (auto v : hyperedges[e])
            v2e[v].push_back(e);

    // prepare hef/hvd
    std::vector<uint32_t> offsets_he(num_edges+1);
    std::vector<vertex_t> hvd;
    uint32_t off = 0;
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<uint32_t>(hyperedges[e].size());
        for (auto v : hyperedges[e])
            hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // prepare vhef/vhed
    std::vector<uint32_t> offsets_vh(num_verts+1);
    std::vector<edge_t> vhed;
    off = 0;
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto &inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<uint32_t>(inc.size());
        for (auto e : inc)
            vhed.push_back(e);
    }
    offsets_vh[num_verts] = off;

    // write files
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hmeta");
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hef");
        f.write(reinterpret_cast<const char*>(offsets_he.data()), offsets_he.size()*sizeof(uint32_t));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write hvd");
        f.write(reinterpret_cast<const char*>(hvd.data()), hvd.size()*sizeof(vertex_t));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhef");
        f.write(reinterpret_cast<const char*>(offsets_vh.data()), offsets_vh.size()*sizeof(uint32_t));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary);
        if (!f) throw std::runtime_error("Unable to write vhed");
        f.write(reinterpret_cast<const char*>(vhed.data()), vhed.size()*sizeof(edge_t));
    }
}

// write .1.cnt color table for subset `used`
static void writeColorTable(const std::string &basename, const std::vector<vertex_t> &used,TreeletTable &origCT) {
    Hypergraph::vertex_t m = static_cast<Hypergraph::vertex_t>(used.size());
    std::ofstream out(basename + ".1.cnt", std::ios::binary);
    if (!out) throw std::runtime_error("Unable to open " + basename + ".1.cnt");

    // write number of vertices
    out.write(reinterpret_cast<const char*>(&m), sizeof(m));

    constexpr std::streamsize buf_size =
        sizeof(Hypergraph::vertex_t)
      + sizeof(uint64_t)
      + sizeof(TreeletTable::treelet_count_pair);
    std::vector<char> buffer(buf_size);
    // count=1
    constexpr uint64_t one = 1;
    memcpy(buffer.data() + sizeof(Hypergraph::vertex_t), &one, sizeof(one));

    TreeletTable::treelet_count_pair tcp;
    for (Hypergraph::vertex_t new_id = 0; new_id < m; ++new_id) {
        // record new dense index
        memcpy(buffer.data(), &new_id, sizeof(new_id));
        // recover original color
        auto it = origCT.begin(used[new_id]);
        tcp.treelet = it.treelet();
        tcp.count   = 1;
        // pack tcp
        memcpy(buffer.data() + sizeof(Hypergraph::vertex_t) + sizeof(one), &tcp, sizeof(tcp));
        out.write(buffer.data(), buf_size);
    }
    out.close();
}

int main(int argc, const char** argv) {
    OptionsParser op;
    auto* help_opt    = op.add_option(false, false, "help",          'h', "",   "Print help and exit");
    auto* input_opt   = op.add_option(true,  true,  "input",         'i', "",   "Input binary hypergraph basename");
    auto* thresh_opt  = op.add_option(true,  true,  "threshold",     't', "0",  "Maximum hyperedge size for the small hypergraph");
    auto* small_opt   = op.add_option(true,  true,  "small-output",  's', "",   "Output basename for hyperedges of size <= threshold");
    auto* large_opt   = op.add_option(true,  true,  "large-output",  'l', "",   "Output basename for hyperedges of size > threshold");
    //auto* colorOpt    = op.add_option(false,  true,  "color-table", 'c', "", "Basename of size-1 color .dtz table");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Missing required options";
        return EXIT_FAILURE;
    }

    const std::string in_base   = input_opt->get_value();
    const vertex_t    threshold = static_cast<vertex_t>(std::stoul(thresh_opt->get_value()));
    const std::string out_small = small_opt->get_value();
    const std::string out_large = large_opt->get_value();
    //const bool        doColor   = colorOpt->is_found();
    //const std::string colorTbl  = doColor ? colorOpt->get_value() : "";

    try {
        Hypergraph H(in_base);
        std::vector<std::vector<vertex_t>> small_he, large_he;
        edge_t m = H.number_of_hyperedges();
        // Raccogli le liste small_he e large_he
        for (edge_t e = 0; e < m; ++e) {
            size_t sz = H.hyperedge_size(e);
            std::vector<vertex_t> he(sz);
            for (vertex_t i = 0; i < sz; ++i) he[i] = H.hyperedge_vertex(e, i);
            if (sz <= threshold) small_he.push_back(std::move(he));
            else large_he.push_back(std::move(he));
        }

        // ————————————————
        // Ordiniamo per mantenere gli invarianti:
        /*
        for (auto &he : small_he) std::sort(he.begin(), he.end());
        for (auto &he : large_he) std::sort(he.begin(), he.end());
        auto cmp = [](auto const &a, auto const &b) { return a.size() > b.size(); };
        std::sort(small_he.begin(), small_he.end(), cmp);
        std::sort(large_he.begin(), large_he.end(), cmp);
        */
        // ————————————————
        

        // 5) Write output hypers
        write_hypergraph(out_small, small_he, H.number_of_vertices());
        write_hypergraph(out_large, large_he, H.number_of_vertices());

        /*
        if (doColor)
        {
            CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias> reader;
            reader.open(colorTbl + ".1.dtz");
            TreeletTable origCT(&reader);
            
            writeColorTable(out_small, usedSmall, origCT);
            PropertyStore properties;
            properties.set_bool("Test", true);
            properties.save(std::string(out_small) + ".1.info");
            writeColorTable(out_large, usedHigh, origCT);
            properties.save(std::string(out_large) + ".1.info");
        }
        */
        
        // Load and compute common pairs
        Hypergraph H_large = Hypergraph(out_large);
        Hypergraph H_small = Hypergraph(out_small);

        std::set<std::pair<vertex_t,vertex_t>> common_pairs;

        for (Hypergraph::edge_t e = 0; e < H_small.number_of_hyperedges(); ++e) {
            size_t sz = H_small.hyperedge_size(e);
            for (size_t i = 0; i + 1 < sz; ++i) {
                for (size_t j = i + 1; j < sz; ++j) {
                    Hypergraph::vertex_t v = H_small.hyperedge_vertex(e, i);
                    Hypergraph::vertex_t u = H_small.hyperedge_vertex(e, j);
                    if(common_pairs.count(std::make_pair(v,u)) > 0) continue;
                    for(size_t deg = 0; deg < H_large.vertex_degree(v); ++deg) {
                        Hypergraph::edge_t incHe = H_large.incident_hyperedge(v, deg);
                        if (std::binary_search(large_he[incHe].begin(), large_he[incHe].end(), u)){ //large_he contiene gli id originali, da capire se id iperarco sia lo stesso
                            auto pair = std::make_pair(v,u);
                            common_pairs.insert(pair);
                            break;
                        }
                    }
                }
            }
        }

        // apri file in modalità binaria
        std::ofstream out(out_small + ".pairs", std::ios::binary);
        if(!out) throw std::runtime_error("Impossibile aprire file coppie");

        // (salva prima il numero di coppie
        uint64_t n = common_pairs.size();
        out.write(reinterpret_cast<const char*>(&n), sizeof(n));

        // per ogni coppia, scrivi i due vertex_t
        for(auto const &p : common_pairs) {
            out.write(reinterpret_cast<const char*>(&p.first),  sizeof(p.first));
            out.write(reinterpret_cast<const char*>(&p.second), sizeof(p.second));
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}