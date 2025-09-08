// MIT License
//
// Hypergraph conversion tool: ASCII <-> binary format
//
// Copyright (c) 2025 Your Name
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// THE SOFTWARE IS PROVIDED.

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <cctype>
#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"

// Read input hypergraph from text: each line is a list of vertex IDs separated by non-digit delimiters
void hypergraph2bin(const std::string &input_txt, const std::string &output_basename) {
    std::ifstream in(input_txt);
    if (!in.is_open()) throw std::runtime_error("Could not open file " + input_txt);

    std::vector<std::vector<Hypergraph::vertex_t>> hyperedges;
    std::string line;
    Hypergraph::vertex_t max_vertex = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<Hypergraph::vertex_t> he;
        std::string token;
        for (size_t i = 0; i <= line.size(); ++i) {
            if (i < line.size() && std::isdigit(line[i])) {
                token.push_back(line[i]);
            } else if (!token.empty()) {
                Hypergraph::vertex_t v = static_cast<Hypergraph::vertex_t>(std::stoul(token));
                he.push_back(v);
                max_vertex = std::max(max_vertex, v);
                token.clear();
            }
        }
        if (!he.empty()) {
            std::sort(he.begin(), he.end());
            he.erase(std::unique(he.begin(), he.end()), he.end());
            hyperedges.push_back(std::move(he));
        }
    }
    in.close();

    // 2) Ordino gli iperarchetti in ordine DECRESCENTE per dimensione
    std::sort(hyperedges.begin(), hyperedges.end(),
        [](auto const &a, auto const &b) {
            return a.size() > b.size();
        }
    );

    Hypergraph::vertex_t num_verts = max_vertex + 1;
    Hypergraph::edge_t num_edges = static_cast<Hypergraph::edge_t>(hyperedges.size());

    // build invert mapping: vertex -> incident hyperedges
    std::vector<std::vector<Hypergraph::edge_t>> v2e(num_verts);
    for (Hypergraph::edge_t e = 0; e < num_edges; ++e) {
        for (auto v : hyperedges[e]) {
            v2e[v].push_back(e);
        }
    }

    // prepare offsets and data arrays
    std::vector<uint32_t> offsets_he(num_edges + 1);
    std::vector<Hypergraph::vertex_t> hvd;
    hvd.reserve(hyperedges.size() * 4);
    uint32_t off = 0;
    for (size_t i = 0; i < hyperedges.size(); ++i) {
        offsets_he[i] = off;
        off += static_cast<uint32_t>(hyperedges[i].size());
        for (auto v : hyperedges[i]) hvd.push_back(v);
    }
    offsets_he[num_edges] = off;

    // vertex -> hyperedge offsets and data
    std::vector<uint32_t> offsets_vh(num_verts + 1);
    std::vector<Hypergraph::edge_t> vhed;
    vhed.reserve(v2e.size() * 4);
    off = 0;
    for (size_t v = 0; v < num_verts; ++v) {
        offsets_vh[v] = off;
        auto &inc = v2e[v];
        std::sort(inc.begin(), inc.end());
        for (auto e : inc) vhed.push_back(e);
        off += static_cast<uint32_t>(inc.size());
    }
    offsets_vh[num_verts] = off;

    // write files
    {
        std::ofstream fmeta(output_basename + ".hmeta", std::ios::binary | std::ios::trunc);
        if (!fmeta) throw std::runtime_error("Unable to write metadata");
        fmeta.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
        fmeta.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
    }
    {
        std::ofstream fhef(output_basename + ".hef", std::ios::binary | std::ios::trunc);
        if (!fhef) throw std::runtime_error("Unable to write offsets_he");
        fhef.write(reinterpret_cast<const char*>(offsets_he.data()), offsets_he.size() * sizeof(uint32_t));
    }
    {
        std::ofstream fhvd(output_basename + ".hvd", std::ios::binary | std::ios::trunc);
        if (!fhvd) throw std::runtime_error("Unable to write hvd");
        fhvd.write(reinterpret_cast<const char*>(hvd.data()), hvd.size() * sizeof(Hypergraph::vertex_t));
    }
    {
        std::ofstream fvhef(output_basename + ".vhef", std::ios::binary | std::ios::trunc);
        if (!fvhef) throw std::runtime_error("Unable to write offsets_vh");
        fvhef.write(reinterpret_cast<const char*>(offsets_vh.data()), offsets_vh.size() * sizeof(uint32_t));
    }
    {
        std::ofstream fvhed(output_basename + ".vhed", std::ios::binary | std::ios::trunc);
        if (!fvhed) throw std::runtime_error("Unable to write vhed");
        fvhed.write(reinterpret_cast<const char*>(vhed.data()), vhed.size() * sizeof(Hypergraph::edge_t));
    }
}

// Read binary hypergraph and dump as ASCII, one hyperedge per line, comma-separated
void bin2hyper(const std::string &basename, const std::string &output_txt) {
    Hypergraph H(basename);
    std::ofstream out(output_txt);
    if (!out) throw std::runtime_error("Unable to write ASCII output");
    for (Hypergraph::edge_t e = 0; e < H.number_of_hyperedges(); ++e) {
        auto sz = H.hyperedge_size(e);
        for (Hypergraph::vertex_t i = 0; i < sz; ++i) {
            out << H.hyperedge_vertex(e, i);
            if (i + 1 < sz) out << ',';
        }
        out << '\n';
    }
}

int main(int argc, const char** argv) {
    OptionsParser op;
    auto* help_opt   = op.add_option(false, false, "help", 'h', "", "Print this help and exit");
    auto* dump_opt   = op.add_option(false, false, "dump", 'd', "false", "Dump binary to ASCII");
    auto* input_opt  = op.add_option(true,  true,  "input", 'i', "", "Input file (ASCII or basename if --dump)");
    auto* output_opt = op.add_option(true,  true,  "output", 'o', "", "Output basename (binary) or file (ASCII)");

    if (!op.parse(argc, argv) || help_opt->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help_opt->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Required options missing\n";
        return EXIT_FAILURE;
    }

    try {
        bool dump = dump_opt->is_found();
        std::string in  = input_opt->get_value();
        std::string out = output_opt->get_value();

        if (dump) {
            bin2hyper(in, out);
        } else {
            hypergraph2bin(in, out);
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
