/*
 * common.h
 *
 *  Created on: 24 mag 2018
 *      Author: brix
 */

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
 * The probability that a coloring with c colors makes k <= c nodes colorful
 */
double pcol(unsigned int k, unsigned int c);

/**
 * Binomial coefficient with *some* care for numeric stability.
 */
double binomial(unsigned long n, unsigned long m); //FIXME: types?


#endif /* SRC_COMMON_UTIL_H_ */
