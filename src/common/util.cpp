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

#include <cassert>
#include "platform/platform.h"

unsigned int uint128_bits_needed(uint128_t n)
{
    unsigned int needed = 1;
    for (n >>= 1; n != 0; n >>= 1)
        needed++;

    return needed;
}

std::string uint128_to_string(uint128_t n)
{
    constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
    constexpr uint128_t ten_38 = ten_19 * ten_19; //Maximum power of 10 representable with an uint128_t

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

uint128_t string_to_uint128(const std::string &s)
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

/**
 * The k-colorful probability for coloring distribution D
 */
double pcold(const double* D, int k)
{
    double p = 1;
    for (int i = 0; i < k; i++)
        p *= D[i] * (i+1);
    return p;
}

/**
 * Normalize entries to have sum s
 */
void normalize(double *v, int k, double s)
{
    double s0 = 0;
    for (int i = 0; i < k; i++)
        s0 += v[i];
    for (int i = 0; i < k; i++)
        v[i] *= s/s0;
}

/**
 * The distribution where each one of the first j elements has probability p/j,
 * and each one of the last k-j elements has probability (1-p)/(k-j)
 */
void bimodal_distribution(double *buf, int k, int j, double p)
{
    for (int i = 0; i < j; i++)
        buf[i] = p/j;
    for (int i = j; i < k; i++)
        buf[i] = (1-p)/(k-j);
    normalize(buf, k, 1.0);
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

bool double_equality(const double x, const double y)
{
    return (x<=y) && (x>=y);
}
