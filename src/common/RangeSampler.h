//
// Created by steven on 3/20/18.
//

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

	const bool compact = true;
	range* ranges;
	uint64_t size;
	uint64_t capacity;

	T total_length;

public:
	RangeSampler(bool compact = true) :
			size(0), capacity(2), total_length(0), compact(compact) {
		ranges = new range[capacity];
	}

	~RangeSampler() {
		delete[] ranges;
	}

	T get_total_length() {
		return total_length;
	}

	void add_range(T from, T to_exclusive) {
		total_length += to_exclusive - from;

		if (compact && size != 0 && ranges[size - 1].to_exclusive == from) {
			ranges[size - 1].to_exclusive = to_exclusive;
			return;
		}

		if (size == capacity) {
			capacity *= 2;
			range* new_ranges = new range[capacity];
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

	/**
	 * Returns index  i  with probability proportional to the size of the i-th range
	 */
	int sample_idx(Random *rng) {
		assert(total_length>0);
		T rand = rng->random_uint<T>(0, total_length-1); //FIXME: Handle empty total length
		range* r = ranges;
		while (rand >= r->to_exclusive - r->from || (r->to_exclusive - r->from) == 0) {
			std::cout << rand << " " << r->from << "-" << r->to_exclusive << std::endl;
			rand -= r->to_exclusive - r->from;
			r++;
		}
		std::cout << "chosen range " << (r-ranges) << " with values " << r->from << "-" << r->to_exclusive << std::endl;
		return r - ranges;
	}

};

#endif //MOTIVO_RANGESAMPLER_H
