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
 * The k-colorful probability when color 0 has probability q.
 */
double pcol(const unsigned int k, double q);

/**
 * The k-colorful probability for coloring distribution D
 */
double pcold(double* D, int k);

/**
 * The distribution where each one of the first j elements has probability p/j,
 * and each one of the last k-j elements has probability (1-p)/(k-j)
 */
void bimodal_distribution(double *buf, int k, int j, double p);

/**
 * Find the bimodal distribution (see above) that gives k-colorful probability
 * equal to alpha * k! / k^k.
 */
void bimodal_distribution_find(double *buf, int k, int j, double alpha);

/**
 * The probability that a coloring with c colors makes k <= c nodes colorful
 */
double pcol(unsigned int k, unsigned int c);

/**
 * Normalize entries to have sum s
 */
void normalize(double *v, int k, double s = 1);

/**
 * Suppose we want a k-colorful probability equal to alpha times the
 * standard k-colorful probability (which is k!/k^k).
 * This method tells the distribution of colors to use, or more precisely,
 * the probability of color 0, assuming all other colors are uniform
 * over the remaining probability.
 */
double colprob_to_prob0(const unsigned int k, double alpha);

/**
 * The probability that a coloring with bias b makes k nodes colorful
 */
double pcolb(unsigned int k, double b);

double col_err(double q, int k, double alpha);
double col_err_deriv(double q0, int k);
double colprob_to_prob0(const unsigned int k, double alpha);


/**
 * Binomial coefficient with *some* care for numeric stability.
 */
double binomial(unsigned long n, unsigned long m); //FIXME: types?


#endif /* SRC_COMMON_UTIL_H_ */
