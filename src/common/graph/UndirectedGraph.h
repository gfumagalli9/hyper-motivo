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

#ifndef MOTIVO_UNDIRECTEDGRAPH_H
#define MOTIVO_UNDIRECTEDGRAPH_H

#include <limits>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <stdexcept>  // NEW: overflow/error handling

/// Represents an immutable undirected unweighted graph.
/// Vertices are numbered with consecutive integers, starting from 0.
///
/// File formats supported:
///   - BASE.gof  : [uint32 n][uint32 E][offsets[0..n] as uint32]
///   - BASE.gof64: [uint32 n][uint64 E][offsets[0..n] as uint64]
/// In both cases, BASE.ged stores neighbors as uint32 vertex IDs.
class UndirectedGraph {
public:
    typedef uint32_t vertex_t;
    static constexpr vertex_t INVALID_VERTEX = std::numeric_limits<vertex_t>::max();

private:
    // Graph sizes
    vertex_t  num_verts{};
    uint64_t  num_edges64{};          // supports large graphs; replaces old uint32

    // Files and mmaps
    FILE *offsets_fd{nullptr};
    FILE *edges_fd{nullptr};
    char *offsets_base{nullptr};      // mmap base for offsets file
    char *offsets{nullptr};           // pointer to the start of offsets array (after header)
    char *edges{nullptr};             // mmap base for edges file

    // Layout/state
    bool     wide_offsets{false};     // true if reading .gof64 (uint64 offsets)
    size_t   offsets_map_bytes{0};    // total mapped size for offsets file
    size_t   offsets_header_bytes{0}; // bytes to skip to reach offsets[0]
    size_t   edges_map_bytes{0};      // total mapped size for edges file

    // Read offset[v] (as an index into BASE.ged) in 32 or 64-bit layout.
    uint64_t get_offset(vertex_t v) const {
        assert(v <= num_verts);
        if (!wide_offsets) {
            uint32_t off32;
            std::memcpy(&off32, offsets + sizeof(uint32_t) * static_cast<uint64_t>(v), sizeof(uint32_t));
            return static_cast<uint64_t>(off32);
        } else {
            uint64_t off64;
            std::memcpy(&off64, offsets + sizeof(uint64_t) * static_cast<uint64_t>(v), sizeof(uint64_t));
            return off64;
        }
    }

    // Address of neighbor i in the adjacency of vertex v (as a raw byte pointer).
    char *offset_of(const vertex_t v, vertex_t i = 0) const
    {
        const uint64_t off = get_offset(v) + static_cast<uint64_t>(i);
        // neighbors are stored as uint32 vertex IDs
        return edges + off * sizeof(vertex_t);
    }

public:
    UndirectedGraph(const UndirectedGraph &) = delete;
    void operator=(const UndirectedGraph &) = delete;

    explicit UndirectedGraph(const std::string &filename);
    ~UndirectedGraph();

    void prefault();

    /// @returns the number of vertices of the graph
    vertex_t number_of_vertices() const {
        return num_verts;
    }

    /// @returns the number of undirected edges (32-bit version).
    /// Throws if the count does not fit in 32 bits (to avoid silent truncation).
    uint32_t number_of_edges() const {
        if (num_edges64 > std::numeric_limits<uint32_t>::max())
            throw std::overflow_error("number_of_edges does not fit in 32-bit");
        return static_cast<uint32_t>(num_edges64);
    }

    /// @returns the number of undirected edges as 64-bit.
    uint64_t number_of_edges64() const {
        return num_edges64;
    }

    /// @returns the degree of vertex @param v
    vertex_t degree(const vertex_t v) const {
        assert(v < num_verts);
        return static_cast<vertex_t>(
            static_cast<uintptr_t>(offset_of(v + 1) - offset_of(v)) / sizeof(vertex_t)
        );
    }

    /// @returns the @param i-th (0 based) neighbor of @param u
    vertex_t neighbor(const vertex_t u, const vertex_t i) const {
        assert(u < num_verts);
        assert(i < degree(u));
        vertex_t v;
        std::memcpy(&v, offset_of(u, i), sizeof(vertex_t));
        return v;
    }

    /// @returns true iff there is an edge between vertex @param u and vertex @param v
    bool has_edge(vertex_t u, vertex_t v) const;

    /// @returns true if the loaded .gof variant uses 64-bit offsets
    bool offsets_are_wide() const { return wide_offsets; }
};

#endif // MOTIVO_UNDIRECTEDGRAPH_H
