// tests/test_gaifman.cpp
#include "doctest.h"
#include <cstdlib>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iostream>

TEST_CASE("Gaifman graph creation from text hypergraph") {
    // 1) write a small hypergraph to text file
    std::ofstream txt("hg.txt");
    txt << "0,1,2\n2,3,4\n1,2,4\n";
    txt.close();

    // 2) invoke the hypergraph builder CLI (motivo-graph) and Gaifman CLI (motivo-gaifman)
    CHECK(std::system("../../build/bin/motivo-hypergraph -i hg.txt -o hg_base") == 0);
    CHECK(std::system("../../build/bin/motivo-gaifman -i hg_base -o g_base") == 0);

    // 3) read .gof
    std::ifstream gof("g_base.gof", std::ios::binary);
    REQUIRE(gof.good());
    uint32_t nv, ne;
    gof.read(reinterpret_cast<char*>(&nv), sizeof(nv));
    gof.read(reinterpret_cast<char*>(&ne), sizeof(ne));
    // expect 5 vertices, 7 undirected edges
    CHECK(nv == 5u);
    CHECK(ne == 7u);
    std::vector<uint32_t> offsets(nv+1);
    for(uint32_t i = 0; i <= nv; ++i) {
        gof.read(reinterpret_cast<char*>(&offsets[i]), sizeof(offsets[i]));
    }
    gof.close();

    // 4) read .ged
    uint32_t total = offsets[nv];
    std::ifstream ged("g_base.ged", std::ios::binary);
    REQUIRE(ged.good());
    std::vector<uint32_t> data(total);
    ged.read(reinterpret_cast<char*>(data.data()), total * sizeof(uint32_t));
    ged.close();

    // 5) reconstruct adjacency lists
    std::vector<std::vector<uint32_t>> adj(nv);
    for(uint32_t u = 0; u < nv; ++u) {
        for(uint32_t idx = offsets[u]; idx < offsets[u+1]; ++idx) {
            adj[u].push_back(data[idx]);
        }
        std::sort(adj[u].begin(), adj[u].end());
    }

    // 6) expected undirected Gaifman graph:
    // hyperedge {0,1,2} → edges 0–1,0–2,1–2
    // {2,3,4} → 2–3,2–4,3–4
    // {1,2,4} → 1–2,1–4,2–4
    // combined unique undirected edges = 7:
    // 0: {1,2}
    // 1: {0,2,4}
    // 2: {0,1,3,4}
    // 3: {2,4}
    // 4: {1,2,3}
    CHECK(adj[0] == std::vector<uint32_t>{1,2});
    CHECK(adj[1] == std::vector<uint32_t>{0,2,4});
    CHECK(adj[2] == std::vector<uint32_t>{0,1,3,4});
    CHECK(adj[3] == std::vector<uint32_t>{2,4});
    CHECK(adj[4] == std::vector<uint32_t>{1,2,3});
}