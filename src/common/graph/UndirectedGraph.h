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

///Represents an immutable undirected unweighted graph
///Vertices are numbered with consecutive integers, starting from 0
class UndirectedGraph {
public:
	typedef uint32_t vertex_t;
	static constexpr vertex_t INVALID_VERTEX = std::numeric_limits<vertex_t>::max();

private:
	vertex_t num_verts;
	uint32_t num_edges;
	FILE *offsets_fd;
	FILE *edges_fd;
	char *offsets;
	char *edges;

	char *offset_of(const vertex_t v, vertex_t i = 0) const
	{
		uint32_t offset;
		memcpy(&offset, offsets + sizeof(uint32_t) * static_cast<uint64_t>(v), sizeof(uint32_t));
		return edges + static_cast<uint64_t>(offset + i) * sizeof(vertex_t);
	}

public:
	UndirectedGraph(const UndirectedGraph &) = delete;
	void operator=(const UndirectedGraph &) = delete;

	explicit UndirectedGraph(const std::string &filename);

	~UndirectedGraph();

	void prefault();

	///@returns the number of vertices of the graph
	vertex_t number_of_vertices() const {
		return num_verts;
	}
	;

	///@returns the number of edges of the graph
	uint32_t number_of_edges() const {
		return num_edges;
	}
	;

	///@returns the degree of vertex @param v
	vertex_t degree(const vertex_t v) const {
		assert(v < num_verts);
		return static_cast<vertex_t>(static_cast<uintptr_t>(offset_of(v + 1) - offset_of(v))
				/ sizeof(vertex_t));
	}

	///@returns the @param i-th (0 based) neighbor of @param u
	vertex_t neighbor(const vertex_t u, const vertex_t i) const {
		assert(u < num_verts);
		assert(i < degree(u));
		vertex_t v;
		memcpy(&v, offset_of(u, i), sizeof(vertex_t));
		return v;
	}

	///@returns true iff there is an edge between vertex @param u and vertex @param v
	bool has_edge(vertex_t u, vertex_t v) const;
};

#endif //MOTIVO_UNDIRECTEDGRAPH_H
