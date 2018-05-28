/*
 * DiscreteDistribution.h
 *
 *  Created on: 25 mag 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_DISCRETEDISTRIBUTION_H_
#define SRC_COMMON_DISCRETEDISTRIBUTION_H_

#include <cstdint>
#include <algorithm>
#include "Random.h"

class DiscreteDistribution {
private:
	double* weights;
	uint64_t size;
	uint64_t capacity;
	double total_weight;

public:
	DiscreteDistribution() :
			size(0), capacity(2), total_weight(0) {
		weights = new double[capacity];
	}

	~DiscreteDistribution() {
	}

	double get_total_weight() {
		return total_weight;
	}

	void add_bin(double weight) {
		total_weight += weight;
		// make space
		if (size == capacity) {
			capacity *= 2;
			double* new_weights = new double[capacity];
			std::copy(weights, weights + size, new_weights);
			delete[] weights;
			weights = new_weights;
		}
		// insert the new bin
		weights[size++] = {weight};
	}

	/**
	 * Returns index  i  with probability proportional to the size of the i-th range
	 */
	uint64_t sample(Random* rng) {
		assert(total_weight > 0);
		double x = total_weight * ((double)rng->random_uint(0, RAND_MAX) / RAND_MAX);
		double* r = weights;
		while(x >= *r) {
			x -= *r;
			r++;
		}
		return r - weights;
	}

	/**
	 * Draw many samples (in an efficient way).
	 */
	void sample(Random* rng, uint64_t* buf, int how_many) {
		assert(total_weight > 0);
		double* dbuf = new double[how_many];
		for (int i = 0; i < how_many; i++) {
			dbuf[i] = total_weight * ((double)rng->random_uint(0, RAND_MAX-1) / RAND_MAX);
		}
		std::sort(dbuf, dbuf + how_many);
		double* r = weights;
		double offset = 0;
		double x = 0;
		for (int i = 0; i < how_many; i++) {
			x = dbuf[i] - offset; // invariant: x = dbuf[i] - (the sum of the *r seen so far)
			while (x >= *r) { // advance till r is such that dbuf[i] - (the sum of the *r seen so far) < next *r
				x -= *r;
				offset += *r;
				r++;
			}
			buf[i] = r - weights;
		}
	}

};

#endif /* SRC_COMMON_DISCRETEDISTRIBUTION_H_ */
