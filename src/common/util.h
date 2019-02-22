/*
 * common.h
 *
 *  Created on: 24 mag 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_UTIL_H_
#define SRC_COMMON_UTIL_H_

#include "../platform/platform.h"

unsigned int uint128_bits_needed(uint128_t n);

/**
 * Convert uint128_t to its decimal string representation.
 */
std::string uint128_to_string(uint128_t n);

/**
 * Convert a string to uint128_t
 */
uint128_t atoi128(const std::string &s);

/**
 * The probability that a coloring with c colors makes k <= c nodes colorful
 */
double pcol(unsigned int k, unsigned int c);

/**
 * The probability that a coloring with bias b makes k nodes colorful
 */
double pcolb(unsigned int k, double b);

/**
 * Binomial coefficient with *some* care for numeric stability.
 */
double binomial(unsigned long n, unsigned long m); //FIXME: types?


#endif /* SRC_COMMON_UTIL_H_ */
