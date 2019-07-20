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

#ifndef MOTIVO_RANGESAMPLER_H
#define MOTIVO_RANGESAMPLER_H

#include <cstdint>
#include <algorithm>
#include "Random.h"

template<typename T> class RangeSampler {
private:

	struct range {
		T from;
		T to_exclusive;
	};

	range* ranges;
	uint64_t size;
	uint64_t capacity;

	T total_length;

	const bool compact;

public:
	explicit RangeSampler(bool compact = true) : size(0), capacity(2), total_length(0), compact(compact)
	{
		ranges = new range[capacity];
	}

	~RangeSampler()
	{
		delete[] ranges;
	}

	T get_total_length() { return total_length; }

	void add_range(T from, T to_exclusive)
	{
		assert(to_exclusive>=from);

		if(from==to_exclusive)
			return;

		total_length += to_exclusive - from;

		if (compact && size != 0 && ranges[size - 1].to_exclusive == from)
		{
			ranges[size - 1].to_exclusive = to_exclusive;
			return;
		}

		if (size == capacity)
		{
			capacity *= 2;
			auto new_ranges = new range[capacity];
			std::copy(ranges, ranges + size, new_ranges);
			delete[] ranges;
			ranges = new_ranges;
		}

		ranges[size++] = {from, to_exclusive};

	}

	T sample(Random *rng)
	{
		assert(total_length>0);
		T rand = rng->random_uint<T>(0, total_length-1); //FIXME: Handle empty total length

		range* r=ranges;
		for(; rand >= r->to_exclusive - r->from; r++)
			rand -= r->to_exclusive - r->from;

		return r->from + rand;
	}
};

#endif //MOTIVO_RANGESAMPLER_H
