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

#ifndef SRC_COMMON_UTIL_H_
#define SRC_COMMON_UTIL_H_

#include "platform/platform.h"
#include <limits>

unsigned int uint128_bits_needed(uint128_t n);

/**
 * Convert uint128_t to its decimal string representation.
 */
std::string uint128_to_string(uint128_t n);

/**
 * Convert a string to uint128_t
 */
uint128_t string_to_uint128(const std::string &s);

/**
 * Compute base^exp as long as the result is at most std::numeric_limits<T>::max
 */
template<typename T> T ipow(T base, unsigned int exp)
{
    static_assert(std::is_unsigned<T>::value, "Type is not unsigned.");

    T result=1;
    while(true)
    {
        if(exp & 0x1)
            result*=base;

        exp>>=1;
        if(exp==0)
            break;

        base*=base;
    }

    return result;
}

/**
 * The k-colorful probability for coloring distribution D
 */
double pcold(const double* D, int k);

/**
 * The distribution where each one of the first j elements has probability p/j,
 * and each one of the last k-j elements has probability (1-p)/(k-j)
 */
void bimodal_distribution(double *buf, int k, int j, double p);

/**
 * Normalize entries to have sum s
 */
void normalize(double *v, int k, double s = 1);

/**
 * The probability that a coloring with c colors makes k <= c nodes colorful
 */
double pcol(unsigned int k, unsigned int c);

/**
 * Binomial coefficient with *some* care for numeric stability.
 */
double binomial(unsigned long n, unsigned long m); //FIXME: types?

//Compares two normal doubles for equality, without generating a warning
bool double_equality [[gnu::const]] (double x, double y);

#endif /* SRC_COMMON_UTIL_H_ */
