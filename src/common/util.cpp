//
// Created by steven on 1/7/19.
//

#include <cassert>
#include "../platform/platform.h"

unsigned int uint128_bits_needed(uint128_t n)
{
    unsigned int needed = 1;
    for (n >>= 1; n != 0; n >>= 1)
        needed++;

    return needed;
}

std::string uint128_to_string(uint128_t n)
{
    static const constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
    static const constexpr uint128_t ten_38 = ten_19 * ten_19; //Maximum power of 10 representable with an uint128_t

    if (n == 0)
        return "0";

    std::string s;
    for (uint128_t max_dec = ten_38; max_dec != 0; max_dec /= 10)
    {
        auto digit = static_cast<unsigned int>(n / max_dec);
        n %= max_dec;
        assert(digit <= 9);

        if (s.length()!=0 || digit != 0)
            s += static_cast<char>('0' + digit);
    }

    return s;
}

uint128_t atoi128(const std::string &s)
{
    uint128_t x = 0;
    for (char c : s)
        x = x*10 + static_cast<unsigned char>(c - '0');

    return x;
}

double pcol(const unsigned int k, const unsigned int c)
{
    if (k > c)
        return 0;

    double p = 1;
    for (unsigned int i = 0; i < k; i++)
        p *= (1 - 1.0 * i / c);

    return p;
}

double pcolb1(const unsigned int k, double b)
{
	// the formula is:  k! * q * ((1-q)/(k-1))^(k-1)
	// where  q = 1/kb  is the probability of color 0 and every other color has prob  (1-q)/(k-1)
    double q = 1/(b*k);
    double p = 1/b;  // equals k*q, now we multiply this by  (k-1)! * ((1-q)/(k-1))^(k-1)
    for (unsigned int i = 1; i <= k-1; i++)
        p *= i*(1-q)/(k-1);
    return p;
}

double pcolb(const unsigned int k, double b)
{
	// the formula is:  k! * q * ((1-q)/(k-1))^(k-1)
	// where  q = kb/(kb+k-1)  is the probability of color 0 and every other color has prob  (1-q)/(k-1)
    double q = b/(b+k-1);
    double p = k*q;  // equals k*q, now we multiply this by  (k-1)! * ((1-q)/(k-1))^(k-1)
    for (unsigned int i = 1; i <= k-1; i++)
      p *= i*(1-q)/(k-1);
    return p;
}

double binomial(const unsigned long n, unsigned long m)
{
    if (n < m)
        return 0;

    if(n-m>m)
        m = n-m;

    double b = 1;
    for (unsigned long i = m + 1; i <= n; i++)
        b *= static_cast<double>(i);

    for (unsigned long i = 2; i <= n - m; i++)
        b /= static_cast<double>(i);

    return b;
}
