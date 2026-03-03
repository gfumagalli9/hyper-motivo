// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
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
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "../../sampler/Occurrence.h"
#include <istream>
#include <fstream>
#include <algorithm>
#include <sys/mman.h>
#include <cerrno>
#include <stdexcept>
#include "UndirectedGraph.h"
#include "../platform/platform.h"

static bool binary_search(const char *begin, const char *end,
                          const UndirectedGraph::vertex_t to_find) {
    UndirectedGraph::vertex_t t;
    while (begin < end) {
        const char* mid = begin
            + static_cast<UndirectedGraph::vertex_t>(
                static_cast<uintptr_t>(end - begin) / (2 * sizeof(UndirectedGraph::vertex_t))
              ) * sizeof(UndirectedGraph::vertex_t);
        std::memcpy(&t, mid, sizeof(UndirectedGraph::vertex_t));

        if (t == to_find) return true;
        if (to_find < t)   end = mid;
        else               begin = mid + sizeof(UndirectedGraph::vertex_t);
    }
    return false;
}

UndirectedGraph::UndirectedGraph(const std::string &basename) {
    // Try legacy .gof (32-bit header + 32-bit offsets).
    std::string offsets_filename = basename + ".gof";
    offsets_fd = std::fopen(offsets_filename.c_str(), "rb");

    if (offsets_fd != nullptr) {
        // --- 32-bit layout: [uint32 n][uint32 E][offsets[0..n] as uint32] ---
        uint32_t n32 = 0, e32 = 0;
        if (std::fread(&n32, sizeof(uint32_t), 1, offsets_fd) != 1)
            throw std::runtime_error("Failed to read n from " + offsets_filename);
        if (std::fread(&e32, sizeof(uint32_t), 1, offsets_fd) != 1)
            throw std::runtime_error("Failed to read E from " + offsets_filename);

        num_verts = static_cast<vertex_t>(n32);
        num_edges64 = static_cast<uint64_t>(e32);
        wide_offsets = false;
        offsets_header_bytes = 2 * sizeof(uint32_t);
        offsets_map_bytes = offsets_header_bytes + (static_cast<size_t>(num_verts) + 1u) * sizeof(uint32_t);
    } else {
        // Fall back to .gof64 (32-bit n + 64-bit E + 64-bit offsets).
        offsets_filename = basename + ".gof64";
        offsets_fd = std::fopen(offsets_filename.c_str(), "rb");
        if (offsets_fd == nullptr)
            throw std::runtime_error("Could not open file " + basename + ".gof nor " + basename + ".gof64");

        // --- 64-bit layout: [uint32 n][uint64 E][offsets[0..n] as uint64] ---
        uint32_t n32 = 0;
        uint64_t e64 = 0;
        if (std::fread(&n32, sizeof(uint32_t), 1, offsets_fd) != 1)
            throw std::runtime_error("Failed to read n from " + offsets_filename);
        if (std::fread(&e64, sizeof(uint64_t), 1, offsets_fd) != 1)
            throw std::runtime_error("Failed to read E from " + offsets_filename);

        num_verts = static_cast<vertex_t>(n32);
        num_edges64 = e64;
        wide_offsets = true;
        offsets_header_bytes = sizeof(uint32_t) + sizeof(uint64_t); // 12 bytes
        offsets_map_bytes = offsets_header_bytes + (static_cast<size_t>(num_verts) + 1u) * sizeof(uint64_t);
    }

    // Map offsets file (exact computed length)
    offsets_base = static_cast<char*>(motivo_mmap(offsets_map_bytes, PROT_READ, fileno(offsets_fd)));
    assert(offsets_base != MAP_FAILED);
    offsets = offsets_base + offsets_header_bytes;

    // Open and map edges file (.ged), which stores directed entries (two per undirected edge)
    std::string edges_filename = basename + ".ged";
    edges_fd = std::fopen(edges_filename.c_str(), "rb");
    if (edges_fd == nullptr)
        throw std::runtime_error("Could not open file " + edges_filename);

    // Total directed entries = 2 * E; each entry is a uint32 vertex ID
    const uint64_t directed_entries = num_edges64 * 2ULL;
    // Check for overflow when mapping (size_t may be 32 or 64 depending on platform)
    const uint64_t bytes_needed = directed_entries * static_cast<uint64_t>(sizeof(vertex_t));
    if (bytes_needed > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        throw std::runtime_error("Edges file is too large to map on this platform");
    }
    edges_map_bytes = static_cast<size_t>(bytes_needed);

    edges = static_cast<char*>(motivo_mmap(edges_map_bytes, PROT_READ, fileno(edges_fd)));
    assert(edges != MAP_FAILED);
}

UndirectedGraph::~UndirectedGraph() {
    if (offsets_fd != nullptr) { // was mmapped
        motivo_munmap(offsets_base, offsets_map_bytes);
        std::fclose(offsets_fd);
    } else { // was allocated (not used in current code path)
        delete[] offsets;
    }
    if (edges_fd != nullptr) { // was mmapped
        motivo_munmap(edges, edges_map_bytes);
        std::fclose(edges_fd);
    } else { // was allocated (not used in current code path)
        delete[] edges;
    }
}

bool UndirectedGraph::has_edge(const vertex_t u, const vertex_t v) const {
    assert(u < num_verts);
    assert(v < num_verts);

    const char* begin_u = offset_of(u);
    const char* end_u   = offset_of(u + 1);
    const char* begin_v = offset_of(v);
    const char* end_v   = offset_of(v + 1);

    if (end_u - begin_u <= end_v - begin_v)
        return binary_search(begin_u, end_u, v);
    else
        return binary_search(begin_v, end_v, u);
}

void UndirectedGraph::prefault() {
    // Touch all pages of the mapped regions to reduce major faults during traversal.
    motivo_prefault(0, offsets_map_bytes, fileno(offsets_fd));
    motivo_prefault(0, edges_map_bytes,   fileno(edges_fd));
}
