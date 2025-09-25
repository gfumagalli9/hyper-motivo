// MIT License
//
// Hypergraph conversion tool: ASCII <-> binary format (Motivo layout)
//
// - ASCII -> binary:  each input line encodes one hyperedge as a list of
//   non-negative integer vertex IDs separated by any NON-digit characters.
//   We extract digit runs (0-9), convert to vertex IDs, sort+deduplicate
//   within the edge, then (globally) sort edges by decreasing size before
//   writing the Motivo binary (.hmeta/.hef/.hvd/.vhef/.vhed).
//
// - Binary -> ASCII: dumps each hyperedge (comma-separated vertex IDs) per line.
//
// Notes / assumptions:
//   * Vertex IDs are non-negative integers (no signs). Any non-digit breaks a token.
//   * Within each hyperedge, duplicates are removed (determinism + compactness).
//   * Edges are globally sorted by size descending (matches your original code).
//   * Output binary format matches the Motivo Hypergraph reader/writer:
//       .hmeta : [num_vertices][num_edges]
//       .hef   : offsets (size: num_edges+1) for edge->vertex adjacency
//       .hvd   : concatenated vertex IDs for all edges
//       .vhef  : offsets (size: num_vertices+1) for vertex->edge incidence
//       .vhed  : concatenated edge IDs for all vertices
//
// Usage:
//   motivo-hypergraph --input <txt-file> --output <basename>
//   motivo-hypergraph --dump  --input <basename> --output <txt-file>
//
// Exit codes:
//   0 on success; nonzero on error.
//
// Copyright (c) 2025
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"

// ---------- ASCII -> memory (vector of edges) ----------

static std::vector<std::vector<Hypergraph::vertex_t>>
read_hyperedges_from_text(const std::string& input_txt)
{
    using vertex_t = Hypergraph::vertex_t;

    std::ifstream in(input_txt);
    if (!in) throw std::runtime_error("Could not open input text file: " + input_txt);

    std::vector<std::vector<vertex_t>> edges;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::vector<vertex_t> he;
        std::string token;

        // Extract contiguous digit runs; any non-digit is a separator.
        for (std::size_t i = 0; i <= line.size(); ++i) {
            const bool is_digit = (i < line.size()) && std::isdigit(static_cast<unsigned char>(line[i]));
            if (is_digit) {
                token.push_back(line[i]);
            } else if (!token.empty()) {
                // Convert run to vertex id; stoul is fine for non-negative ints.
                const vertex_t v = static_cast<vertex_t>(std::stoul(token));
                he.push_back(v);
                token.clear();
            }
        }

        if (!he.empty()) {
            // Normalize each edge: sort + unique
            std::sort(he.begin(), he.end());
            he.erase(std::unique(he.begin(), he.end()), he.end());
            edges.push_back(std::move(he));
        }
    }
    return edges;
}

// ---------- memory -> Motivo binary writer ----------

static void write_hypergraph_bin(
    const std::string& basename,
    const std::vector<std::vector<Hypergraph::vertex_t>>& edges_sorted_desc)
{
    using vertex_t = Hypergraph::vertex_t;
    using edge_t   = Hypergraph::edge_t;

    // Infer vertex count from max vertex id present; empty means 0.
    vertex_t max_v = 0;
    std::size_t total_edge_degree = 0;
    for (const auto& he : edges_sorted_desc) {
        total_edge_degree += he.size();
        for (vertex_t v : he) max_v = std::max(max_v, v);
    }
    const vertex_t num_verts = (edges_sorted_desc.empty() ? 0 : static_cast<vertex_t>(max_v + 1));
    const edge_t   num_edges = static_cast<edge_t>(edges_sorted_desc.size());

    // Build vertex -> incident edges
    std::vector<std::vector<edge_t>> v2e(num_verts);
    for (edge_t e = 0; e < num_edges; ++e) {
        for (vertex_t v : edges_sorted_desc[e]) v2e[v].push_back(e);
    }

    // Prepare CSR-like arrays
    // Edge->Vertex (HEF/HVD)
    std::vector<std::uint32_t> offsets_he(num_edges + 1);
    std::vector<vertex_t>      hvd;
    hvd.reserve(total_edge_degree);

    std::uint32_t off = 0;
    for (edge_t e = 0; e < num_edges; ++e) {
        offsets_he[e] = off;
        off += static_cast<std::uint32_t>(edges_sorted_desc[e].size());
        for (vertex_t v : edges_sorted_desc[e]) hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // Vertex->Edge (VHEF/VHED), keep incident edge lists sorted for determinism
    std::vector<std::uint32_t> offsets_vh(num_verts + 1);
    std::vector<edge_t>        vhed;
    {
        std::size_t tot = 0;
        for (const auto& inc : v2e) tot += inc.size();
        vhed.reserve(tot);
    }

    off = 0;
    for (vertex_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto& inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        off += static_cast<std::uint32_t>(inc.size());
        for (edge_t e : inc) vhed.push_back(e);
    }
    offsets_vh[num_verts] = off;

    // Write files
    {
        std::ofstream f(basename + ".hmeta", std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Unable to write " + basename + ".hmeta");
        f.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        f.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream f(basename + ".hef", std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Unable to write " + basename + ".hef");
        f.write(reinterpret_cast<const char*>(offsets_he.data()),
                static_cast<std::streamsize>(offsets_he.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".hvd", std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Unable to write " + basename + ".hvd");
        f.write(reinterpret_cast<const char*>(hvd.data()),
                static_cast<std::streamsize>(hvd.size() * sizeof(vertex_t)));
    }
    {
        std::ofstream f(basename + ".vhef", std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Unable to write " + basename + ".vhef");
        f.write(reinterpret_cast<const char*>(offsets_vh.data()),
                static_cast<std::streamsize>(offsets_vh.size() * sizeof(std::uint32_t)));
    }
    {
        std::ofstream f(basename + ".vhed", std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Unable to write " + basename + ".vhed");
        f.write(reinterpret_cast<const char*>(vhed.data()),
                static_cast<std::streamsize>(vhed.size() * sizeof(edge_t)));
    }
}

// ---------- ASCII -> binary (driver) ----------

static void hypergraph2bin(const std::string& input_txt, const std::string& output_basename)
{
    // 1) Read ASCII edges
    auto edges = read_hyperedges_from_text(input_txt);

    // 2) Global sort: by decreasing edge size (kept from your original tool)
    std::sort(edges.begin(), edges.end(),
              [](const auto& a, const auto& b){ return a.size() > b.size(); });

    // 3) Write Motivo binary
    write_hypergraph_bin(output_basename, edges);
}

// ---------- binary -> ASCII ----------

static void bin2hyper(const std::string& basename, const std::string& output_txt)
{
    Hypergraph H(basename);
    std::ofstream out(output_txt);
    if (!out) throw std::runtime_error("Unable to write ASCII output: " + output_txt);

    for (Hypergraph::edge_t e = 0; e < H.number_of_hyperedges(); ++e) {
        const auto sz = H.hyperedge_size(e);
        for (Hypergraph::vertex_t i = 0; i < sz; ++i) {
            out << H.hyperedge_vertex(e, i);
            if (i + 1 < sz) out << ',';
        }
        out << '\n';
    }
}

// ---------- CLI ----------

int main(int argc, const char** argv)
{
    OptionsParser op;
    auto* help_opt   = op.add_option(false, false, "help",   'h', "",     "Print this help and exit");
    auto* dump_opt   = op.add_option(false, false, "dump",   'd', "",     "If set, dump binary to ASCII");
    auto* input_opt  = op.add_option(true,  true,  "input",  'i', "",     "Input: text file (default) or basename if --dump");
    auto* output_opt = op.add_option(true,  true,  "output", 'o', "",     "Output: basename (binary) or text file if --dump");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Required options missing\n";
        return EXIT_FAILURE;
    }

    try {
        const bool dump = dump_opt->is_found();
        const std::string in  = input_opt->get_value();
        const std::string out = output_opt->get_value();

        if (dump) {
            bin2hyper(in, out);
        } else {
            hypergraph2bin(in, out);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}