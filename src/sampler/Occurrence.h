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

#ifndef MOTIVO_OCCURRENCE_H
#define MOTIVO_OCCURRENCE_H

#include <climits>
#include "../common/platform/platform.h"
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "include_nauty.h"

class OccurrenceCanonicizer;

class Occurrence
{
	friend class OccurrenceCanonicizer;

public:
    //i,j in {0,...,15}
    //edge (i,j) with i>j is in position sum_{k=1}^(i-1) k + j = (i-1)*i/2 + j in edges
    //last bit is the one corresponding to i=15, j=14 => at most 119 bits are need (14 bytes, 7 bits)
    constexpr static unsigned int binary_footprint_bits = 119;
    constexpr static unsigned int binary_footprint_bytes = (binary_footprint_bits + CHAR_BIT - 1) / CHAR_BIT; //round up (to 15 bytes)
    constexpr static unsigned int text_footprint_bytes = binary_footprint_bytes * 2;

private:
    unsigned int size;
    UndirectedGraph::vertex_t verts[16] = { 0 };
    uint8_t edges[binary_footprint_bytes] = { 0 };
    mutable char text_footprint_buffer[text_footprint_bytes + 1] = { 0 }; //Add null-terminator

    inline void add_edge(unsigned int i, unsigned int j)
    {
        assert(i > j);
        unsigned int pos = (i - 1) * i / 2 + j;
        edges[pos / 8] |= static_cast<uint8_t>(0b10000000 >> (pos % 8));
    }

public:
    constexpr Occurrence() : size(0)
    {} //Empty constructor to take advantage of Stack allocation

	Occurrence(const Treelet& treelet, const UndirectedGraph::vertex_t* occ);
	Occurrence(unsigned int size, const UndirectedGraph* graph, const UndirectedGraph::vertex_t* occ);
	Occurrence(unsigned int size, const uint8_t* edges);

	inline bool has_edge(unsigned int i, unsigned int j) const //FIXME: Could be invoked with j>=i
	{
		assert(i > j);
		unsigned int pos = (i - 1) * i / 2 + j;
		return (edges[pos / 8] & (0b10000000u >> (pos % 8))) != 0;
	}

	const UndirectedGraph::vertex_t* vertices() const
    {
		return verts;
	}

	const char* binary_footprint() const
    {
		return reinterpret_cast<const char*>(edges);
	}

	//Lazily computes the text footprint. Const is fine because the footprint is mutable
	const char* text_footprint() const;

	bool is_valid() const { return size != 0; }

	unsigned int get_size() const { return size;}

    struct OccurrenceFootprintHash
    {
        inline size_t operator()[[gnu::hot,gnu::flatten]] (const Occurrence *key) const
        {
            size_t seed;
            seed = key->is_valid()?0xcb7fedb03a45866f:0xb896186490f1c8e9;

            const char* p = key->binary_footprint();
            for (unsigned int i = 0; i < Occurrence::binary_footprint_bytes; i++)
                seed ^= static_cast<unsigned char>(p[i])*0xff51afd7ed558ccd +0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
        inline size_t operator()[[gnu::hot,gnu::flatten]] (const Occurrence &key) const
        {
            size_t seed;
            seed = key.is_valid()?0xcb7fedb03a45866f:0xb896186490f1c8e9;

            const char* p = key.binary_footprint();
            for (unsigned int i = 0; i < Occurrence::binary_footprint_bytes; i++)
                seed ^= static_cast<unsigned char>(p[i])*0xff51afd7ed558ccd +0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };

    struct OccurrenceFootprintEquality
    {
        inline bool operator() [[gnu::hot,gnu::flatten]] (const Occurrence *occ1, const Occurrence *occ2) const
        {
            return (occ1->is_valid()==occ2->is_valid()) && !memcmp(occ1->binary_footprint(), occ2->binary_footprint(), Occurrence::binary_footprint_bytes);
        }
        inline bool operator() [[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
        {
            return (occ1.is_valid()==occ2.is_valid()) && !memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes);
        }
    };

    struct OccurrenceFootprintLess
    {
        inline bool operator()[[gnu::hot,gnu::flatten]] (const Occurrence *occ1, const Occurrence *occ2) const
        {
            return (occ1->is_valid()==occ2->is_valid()) && (memcmp(occ1->binary_footprint(), occ2->binary_footprint(), Occurrence::binary_footprint_bytes) < 0);
        }
        inline bool operator()[[gnu::hot,gnu::flatten]] (const Occurrence &occ1, const Occurrence &occ2) const
        {
            return (occ1.is_valid()==occ2.is_valid()) && (memcmp(occ1.binary_footprint(), occ2.binary_footprint(), Occurrence::binary_footprint_bytes) < 0);
        }
    };
};

static_assert(std::is_trivially_copyable<Occurrence>::value, "Occurrence is not trivially copyable");
static_assert(std::is_trivially_assignable<Occurrence, Occurrence>::value, "Occurrence is not trivially assignable");
static_assert(std::is_trivially_copy_assignable<Occurrence>::value, "Occurrence is not trivially copy assignable");
static_assert(std::is_trivially_move_assignable<Occurrence>::value, "Occurrence is not trivially move assignable");
static_assert(std::is_trivially_copy_constructible<Occurrence>::value, "Occurrence is not trivially copy constructible");
static_assert(std::is_trivially_move_constructible<Occurrence>::value, "Occurrence is not trivially move constructible");

//The underlying library used to canonicize the occurrence requires initialization and cleanup to be used
//from multiple threads. We use this friend class to save on this overhead.
//A single instance of this class is not thread safe. However distinct instances can be used by different threads.
class OccurrenceCanonicizer
{
private:
	const unsigned int size;
	const size_t words_needed;

	nauty_graph* g;
	nauty_graph *cang;
	int *lab;
	int *ptn;
	int *orbits;

	DEFAULTOPTIONS_GRAPH(options);
	statsblk stats;

public:
	explicit OccurrenceCanonicizer(unsigned int size);
	~OccurrenceCanonicizer();

	void canonicize(Occurrence* occ);
};

#endif //MOTIVO_OCCURRENCE_H
