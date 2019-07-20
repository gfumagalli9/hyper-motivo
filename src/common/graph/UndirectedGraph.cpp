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
#include "UndirectedGraph.h"
#include "../platform/platform.h"

UndirectedGraph::UndirectedGraph(const std::string &basename) {
	std::string offsets_filename = basename + ".gof";
	offsets_fd = fopen(offsets_filename.c_str(), "rb");
	if (offsets_fd == nullptr)
		throw std::runtime_error("Could not open file " + offsets_filename);

	std::string edges_filename = basename + ".ged";
	edges_fd = fopen(edges_filename.c_str(), "rb");
	if (edges_fd == nullptr)
		throw std::runtime_error("Could not open file " + edges_filename);

	fread(&num_verts, sizeof(vertex_t), 1, offsets_fd);
	fread(&num_edges, sizeof(vertex_t), 1, offsets_fd); //FIXME: use own type?
	offsets = static_cast<char*>(motivo_mmap((num_verts + 2) * sizeof(vertex_t),
	PROT_READ, fileno(offsets_fd)));
	assert(offsets!=MAP_FAILED);
	offsets += 2 * sizeof(vertex_t);

	edges = static_cast<char*>(motivo_mmap(2 * num_edges * sizeof(vertex_t),
	PROT_READ, fileno(edges_fd)));
	assert(edges!=MAP_FAILED);
}

UndirectedGraph::~UndirectedGraph() {
	if (offsets_fd != nullptr) { // was mmapped
		motivo_munmap(offsets - 2 * sizeof(vertex_t), (num_verts + 2) * sizeof(vertex_t));
		fclose(offsets_fd);
	} else { // was allocated
		delete[] offsets;
	}
	if (edges_fd != nullptr) { // was mmapped
		motivo_munmap(edges, 2 * num_edges * sizeof(vertex_t));
		fclose(edges_fd);
	} else { // was allocated
		delete[] edges;
	}
}

static bool binary_search(const char *begin, const char *end,
		const UndirectedGraph::vertex_t to_find) {
	UndirectedGraph::vertex_t t;
	while (begin < end) {
		const char* mid = begin
				+ static_cast<UndirectedGraph::vertex_t>(static_cast<uintptr_t>(end - begin)
						/ (2 * sizeof(UndirectedGraph::vertex_t)))
						* sizeof(UndirectedGraph::vertex_t);
		memcpy(&t, mid, sizeof(UndirectedGraph::vertex_t));

		if (t == to_find)
			return true;

		if (to_find < t)
			end = mid;
		else
			begin = mid + sizeof(UndirectedGraph::vertex_t);
	}

	return false;
}

bool UndirectedGraph::has_edge(const vertex_t u, const vertex_t v) const {
	assert(u < num_verts);
	assert(v < num_verts);

	const char* begin_u = offset_of(u);
	const char* end_u = offset_of(u + 1);
	const char* begin_v = offset_of(v);
	const char* end_v = offset_of(v + 1);

	if (end_u - begin_u <= end_v - begin_v)
		return binary_search(begin_u, end_u, v);
	else
		return binary_search(begin_v, end_v, u);
}

void UndirectedGraph::prefault() {
	motivo_prefault(0, (num_verts + 2) * sizeof(vertex_t), fileno(offsets_fd));
	motivo_prefault(0, 2 * num_edges * sizeof(vertex_t), fileno(edges_fd));
}
