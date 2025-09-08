// MIT License
//
// Gaifman graph construction tool: from hypergraph to undirected graph
//
// Copyright (c) 2025 Your Name
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software are
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
// THE SOFTWARE.

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "../common/OptionsParser.h"
#include "../common/graph/Hypergraph.h"

int main(int argc, const char** argv) {
    OptionsParser op;
    auto* help  = op.add_option(false, false, "help", 'h', "", "Print help and exit");
    auto* inopt = op.add_option(true,  true,  "input", 'i', "", "Input hypergraph basename");
    auto* outopt= op.add_option(true,  true,  "output", 'o', "", "Output graph basename");

    if (!op.parse(argc, argv) || help->is_found()) {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n" << op.help();
        return help->is_found() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!op.has_required_options()) {
        std::cerr << "Missing required options\n";
        return EXIT_FAILURE;
    }

    std::string in_base  = inopt->get_value();
    std::string out_base = outopt->get_value();

    try {
        // Load hypergraph
        Hypergraph H(in_base);
        H.prefault();
        auto n = H.number_of_vertices();
        // Build adjacency lists
        std::vector<std::vector<Hypergraph::vertex_t>> adj(n);
        for (Hypergraph::edge_t e = 0; e < H.number_of_hyperedges(); ++e) {
            auto sz = H.hyperedge_size(e);
            std::vector<Hypergraph::vertex_t> he(sz);
            for (size_t i = 0; i < sz; ++i)
                he[i] = H.hyperedge_vertex(e, i);
            // for each pair in hyperedge, add edge
            for (size_t i = 0; i < sz; ++i) {
                for (size_t j = i + 1; j < sz; ++j) {
                    auto u = he[i];
                    auto v = he[j];
                    adj[u].push_back(v);
                    adj[v].push_back(u);
                }
            }
        }
        // Remove duplicates and sort neighbors
        uint64_t total_deg = 0;
        for (auto &nbrs : adj) {
            auto &v = nbrs;
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
            total_deg += v.size();
        }

        // Compute num_edges
        uint64_t processed = total_deg;
        uint64_t num_edges = processed / 2;

        // Write .gof (.gof = graph offsets file)
        {
            std::ofstream f(out_base + ".gof", std::ios::binary | std::ios::trunc);
            if (!f) throw std::runtime_error("Cannot open " + out_base + ".gof");
            uint32_t nv = static_cast<uint32_t>(n);
            uint32_t ne = static_cast<uint32_t>(num_edges);
            f.write(reinterpret_cast<char*>(&nv), sizeof(nv));
            f.write(reinterpret_cast<char*>(&ne), sizeof(ne));
            uint32_t offset = 0;
            for (size_t u = 0; u < n; ++u) {
                f.write(reinterpret_cast<char*>(&offset), sizeof(offset));
                offset += static_cast<uint32_t>(adj[u].size());
            }
            f.write(reinterpret_cast<char*>(&offset), sizeof(offset));
        }
        // Write .ged (edge data)
        {
            std::ofstream f(out_base + ".ged", std::ios::binary | std::ios::trunc);
            if (!f) throw std::runtime_error("Cannot open " + out_base + ".ged");
            for (size_t u = 0; u < n; ++u) {
                for (auto v : adj[u]) {
                    f.write(reinterpret_cast<char*>(&v), sizeof(v));
                }
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}