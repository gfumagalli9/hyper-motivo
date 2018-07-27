/*
 * common.h
 *
 *  Created on: 24 mag 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_COMMON_H_
#define SRC_COMMON_COMMON_H_
#include <unistd.h>
#include <ostream>

inline unsigned int bits_needed(uint128_t n) {
	unsigned int needed = 1;
	for (n >>= 1; n != 0; n >>= 1)
		needed++;

	return needed;
}

/**
 * Convert uint128_t to its decimal string representation.
 */
inline std::string to_string(uint128_t n) {
	static const constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
	static const constexpr uint128_t ten_38 = ten_19 * ten_19; //Maximum power of 10 representable with an uint128_t

	if (n == 0)
		return "0";

	std::string s = "";
	bool significant_digit_found = false;
	for (uint128_t max_dec = ten_38; max_dec != 0; max_dec /= 10) {
		unsigned int digit = static_cast<unsigned int>(n / max_dec);
		n = n % max_dec;
		assert(digit <= 9);
		if (significant_digit_found || digit != 0) {
			significant_digit_found = true;
			s += static_cast<char>('0' + digit);
		}
	}

	return s;
}

inline std::ostream& operator<<(std::ostream& o, const uint128_t n) {
	o << to_string(n);
	return o;
}


/**
 * Convert a string to uint128_t
 */
inline uint128_t atoi128(std::string const s)
{
	uint128_t x = 0;
	for (unsigned int i = 0; i < s.size(); i++)
	{
		x *= 10;
		x += static_cast<unsigned char>(s[i] - '0');
	}
	return x;
}

/**
 * The probability that a coloring with c colors makes k <= c nodes colorful
 */
inline double pcol(unsigned int k, unsigned int c)
{
	if (k > c)
		return 0;

    double p = 1;
    for (unsigned int i = 0; i < k; i++)
        p *= (1 - 1.0 * i / c);

    return p;
}

/**
 * Binomial coefficient with *some* care for numeric stability.
 */
inline double binomial(unsigned long n, unsigned long m) //FIXME: types?
{
	if (n < m)
		return 0;

	double b = 1;
	m = std::max(m, n - m);

	for (unsigned long i = m + 1; i <= n; i++)
		b *= static_cast<double>(i);

	for (unsigned long i = 2; i <= n - m; i++)
		b /= static_cast<double>(i);

	return b;
}

/**
 * Determine the number of open file descriptors
 */
inline int num_open_fd() {
	int j, n = 0;
	// count open file descriptors
	const int FDMAX = 4096;
	for (j = 0; j < FDMAX; ++j) {
		int fd = dup(j);
		if (fd < 0)
			continue;
		++n;
		close(fd);
	}
	return n;
}

#endif /* SRC_COMMON_COMMON_H_ */
