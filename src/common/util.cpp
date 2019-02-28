//
// Created by steven on 1/7/19.
//

#include <cassert>
#include "../platform/platform.h"
#include <cmath>

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


/**
 * The k-colorful probability when color 0 has bias b
 */
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

/**
 * The k-colorful probability when color 0 has probability q
 */
double pcol(const unsigned int k, double q)
{
    double p = q;
    for (unsigned int i = 1; i < k; i++)
        p *= (i+1)*(1-q)/(k-1);
    return p;
}

double col_err(double q, int k, double alpha)
{
  return pcol(k, q) - alpha * pcol(k, 1.0/k);
}

/**
 * Derivative w.r.t. q evaluated at q0
 */
double col_err_deriv(double q0, int k)
{
  return pow(1-q0, k-2) * (1 + q0*(k-2));
}

/**
 * The probability one must give to color 0 to obtain k-colorful probability alpha * (k!/k^k)
 * E.g. colprob_to_prob0(7, 1e-3) gives the probability you should assign to color 0 to see
 * your average treelet count shrink by 1000 times compared to the coloring where each color
 * has probability 1/7.
 */
double colprob_to_prob0(const unsigned int k, double alpha)
{
  double q = 1.0/k;
  double eps = 0.01*alpha*pcol(k, 1.0/k);
  int i = 1000;
  while (i-- && col_err(q, k, alpha) > eps) {
//	std::cout << q << std::endl;
    q += col_err(q, k, alpha)/col_err_deriv(q, k);
  }
  return q;
}


/**
 * The k-colorful probability for coloring distribution D
 */
double pcold(double* D, int k) {
	double p = 1;
	for (int i = 0; i < k; i++)
		p *= D[i] * (i+1);
	return p;
}

/**
 * Normalize entries to have sum s
 */
void normalize(double *v, int k, double s = 1) {
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
void bimodal_distribution(double *buf, int k, int j, double p) {
	for (int i = 0; i < j; i++)
		buf[i] = p/j;
	for (int i = j; i < k; i++)
		buf[i] = (1-p)/(k-j);
	normalize(buf, k, 1.0);
}

/**
 * Find the bimodal distribution (see above) that gives k-colorful probability
 * equal to alpha * k! / k^k.
 */
void bimodal_distribution_find(double *buf, int k, int j, double alpha) {
	double p = 1.0/k;
	bimodal_distribution(buf, k, j, p*j);
	double pk = alpha * pcol(k, 1.0/k);
	double p_lo = 0, p_hi = p;
	int itr = 0;
	while (fabs(pcold(buf, k) - pk) > pk * 1e-30 && itr < 1000) {
		if (pcold(buf, k) > pk) {
			p_hi = p;
			p = (p + p_lo)/2;
		} else {
			p_lo = p;
			p = (p + p_hi)/2;
		}
		bimodal_distribution(buf, k, j, p);
		itr++;
	}
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
