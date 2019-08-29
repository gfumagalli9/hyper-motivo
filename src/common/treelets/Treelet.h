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

#ifndef MOTIVO_TREELET_H
#define MOTIVO_TREELET_H

#include <cstdint>
#include <cassert>
#include <utility>
#include "../platform/platform.h"

//The following classs is already packed.
//See: https://en.wikipedia.org/wiki/Data_structure_alignment#Typical_alignment_of_C_structs_on_x86
class [[gnu::packed]] Treelet
{
	/* Each treelet is represented as a bit string of 48 bits.
	 * 0-31:  Structure of the treelet.
	 * 32-47: Bitmask representing the treelet colors.
	 *
	 * Structure is encode as a DFS traversal, in binary.
	 * 1 means that we entered a new vertex and 0 means we are leaving a vertex and its subtree.
	 * The first bit is always 1 and it is not stored. Bits are left-aligned.
	 * We can represent treelets up to size 16 (using 31 bits).
	 * I.e: a star with 3 leaves is 1010100 followed by 25 zeros, a path with 4 nodes is 1110000 followed by 26 zeros,
	 * a binary tree of height two is 1101001101000 followed by 19 zeros.
	 */
public:
	typedef uint32_t treelet_structure_t;
	typedef uint16_t treelet_colors_t;
	constexpr static int treelet_structure_bits = 32;
	constexpr static treelet_structure_t treelet_structure_highest_bit = 1u << (treelet_structure_bits - 1);
	constexpr static treelet_structure_t invalid_structure = 0xFFFFFFFF;
	constexpr static treelet_structure_t singleton_structure = 0;
	constexpr static treelet_colors_t all_colors = 0xFFFF;

private:
	treelet_structure_t structure;
	uint16_t colors;

    Treelet reroot(unsigned int new_root, const unsigned int* parents, const unsigned int* subtree_bit_start, const unsigned int* subtree_bit_end) const;

public:
	Treelet() noexcept = default; //Required to create treelet array. Does not initialize structure and colors

	explicit constexpr Treelet(treelet_structure_t structure, uint16_t colors = 0) noexcept
		: structure(structure), colors(colors)
	{}

	///@returns the number of vertices of the treelet
	inline unsigned int number_of_vertices() const {
		return static_cast<unsigned int>(popcount32(structure) + 1);
	}

	static unsigned int number_of_vertices(const treelet_structure_t structure)
	{
		return static_cast<unsigned int>(popcount32(structure) + 1);
	}

	struct TreeletHash
    {
		inline size_t operator()[[gnu::hot,gnu::flatten]] (const Treelet t) const
		{
			uint64_t key=0;
			memcpy(&key, &t, sizeof(Treelet));

			//MurmurHash3 finalizer by Austin Appleby (public domain)
			key ^= key >> 33;
			key *= 0xff51afd7ed558ccd;
			key ^= key >> 33;
			key *= 0xc4ceb9fe1a85ec53;
			key ^= key >> 33;

			return key;
		}
	};

	///@returns true iff the represented treelet is invalid, e.g., due to a failed merge
	inline bool is_valid() const {
		return structure != invalid_structure;
	}

	///@returns true iff the treelet is colored
	inline bool is_colored() const {
		return colors != 0;
	}

	///@returns true iff the treelet is colored
	inline bool is_singleton() const {
		return structure == 0;
	}

	///@returns true iff the treelet is colored
	inline treelet_colors_t get_colors() const {
		return colors;
	}

	///Initializes a signleton treelet having color @param color
	inline static Treelet singleton(const uint8_t color)
	{
		return Treelet(singleton_structure, static_cast<uint16_t>(1 << color));
	}

	///Merges the the current treelet with @param other
	///@returns the merged treelet or an invalid treelet if the current treelet and @param t2 are not mergeable
	Treelet merge(Treelet other) const;

	///@returns the number of times a treelet will be overcounted when merging using "merge"
	uint8_t normalization_factor() const;

	///@returns an opaque value representing the structure of the treelet
	inline treelet_structure_t get_structure() const { return structure; }

	Treelet split_child() const;

	Treelet complement(Treelet t2) const;

	Treelet canonical_rooting() const;

	inline bool operator==(const Treelet& other) const
	{
		return structure == other.structure && colors == other.colors;
	}

	inline bool operator!=(const Treelet& other) const
	{
		return structure != other.structure || colors != other.colors;
	}

	inline bool operator<(const Treelet& other) const
	{
		return (structure > other.structure) || (structure == other.structure && colors < other.colors);
	}
	inline bool operator<=(const Treelet& other) const
	{
		return (structure > other.structure) || (structure == other.structure && colors <= other.colors);
	}
};

static_assert(sizeof(Treelet) == 6, "Treelet is not packed in 6 bytes");

constexpr static Treelet invalid_treelet { Treelet::invalid_structure, 0}; //A generic invalid treelet representation
constexpr static Treelet invalid_merge_colors { Treelet::invalid_structure, 1}; //Merge failed due to intersecting colors
constexpr static Treelet invalid_merge_structure { Treelet::invalid_structure, 2}; //Merge failed due to wrong structure order



#endif //MOTIVO_TREELET_H
