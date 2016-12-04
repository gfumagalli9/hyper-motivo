//
// Created by steven on 12/2/16.
//

#ifndef MOTIVO_PLATFORM_H
#define MOTIVO_PLATFORM_H


#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <memory.h>
#include "generated/leftmost_bit_tie_lut.h"

///@returns the number of bits set to 1 in x
#define popcount(x) ( __builtin_popcount( (x) ) )


/*inline int leftmost_bit_tie1(uint32_t x)
{
    __m128i spread = _mm_shuffle_epi8(_mm_setr_epi32(x, x >> 2, x >> 4, x >> 6), _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15));
    spread = _mm_and_si128(spread, _mm_set1_epi8(3));

    __m128i r = _mm_shuffle_epi8(_mm_setr_epi8(-1, 0, 0, 1,  0,0,0,0,0,0,0,0,0,0,0,0), spread);

    __m128i pfs = _mm_add_epi8(r, _mm_srli_si128(r, 1));
    pfs = _mm_add_epi8(pfs, _mm_srli_si128(pfs, 2));
    pfs = _mm_add_epi8(pfs, _mm_srli_si128(pfs, 4));
    pfs = _mm_add_epi8(pfs, _mm_srli_si128(pfs, 8));

    __m128i iszero = _mm_cmpeq_epi8(pfs, _mm_setzero_si128());
    return __builtin_clz(_mm_movemask_epi8(iszero) << 15) * 2;
}*/

///@pre the leftmost bit of x is 1
///@returns the index of the smallest index i>0 such that the number of 0s and 1s in the leftmost i bits of x are equal
inline int leftmost_bit_tie1 [[gnu::pure]] (uint32_t x)
{
    uint_fast16_t y = x>>24 & 0b01111111;
    if(leftmost_bit_tie_LUT0[y] < 0)
        return ~leftmost_bit_tie_LUT0[y];

    y = static_cast<uint_fast16_t>(leftmost_bit_tie_LUT0[y]);
    y= (y<<8) | ((x>>16) & 0xFF);
    if(leftmost_bit_tie_LUT1[y] < 0)
        return ~leftmost_bit_tie_LUT1[y];

    y = static_cast<uint_fast16_t>(leftmost_bit_tie_LUT1[y]);
    y = (y<<8) | ((x>>8) & 0xFF);
    if(leftmost_bit_tie_LUT2[y] < 0)
        return ~leftmost_bit_tie_LUT2[y];

    y = static_cast<uint_fast16_t>(leftmost_bit_tie_LUT2[y]);
    y = (y<<8) | (x & 0xFF);
    return ~leftmost_bit_tie_LUT3[y];
}

///@pre the leftmost bit of x is 0
///@returns the index of the smallest index i>0 such that the number of 0s and 1s in the leftmost i bits of x are equal
inline int leftmost_bit_tie0 [[gnu::pure, gnu::flatten]] (uint32_t x) { return leftmost_bit_tie1(~x); }

///@returns the index of the smallest index i>0 such that the number of 0s and 1s in the leftmost i bits of x are equal
inline int leftmost_bit_tie [[gnu::pure, gnu::flatten]] (uint32_t x) { return leftmost_bit_tie1((x>>31)?x:~x); }


/*
static const unsigned char BitReverseTable256[] =
        {
                0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
                0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
                0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
                0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
                0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
                0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
                0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
                0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
                0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
                0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
                0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
                0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
                0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
                0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
                0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
                0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
        };

inline uint32_t brev(uint32_t x)
{
    return (BitReverseTable256[x & 0xff] << 24) |
           (BitReverseTable256[(x >> 8) & 0xff] << 16) |
           (BitReverseTable256[(x >> 16) & 0xff] << 8) |
           (BitReverseTable256[(x >> 24) & 0xff]);

}
// Return the number of most significant (leftmost) bits that must be extracted
//   to achieve an equal count of 1-bits and 0-bits in the extracted bit group.
//   Return 0 if no such bit group exists.
inline int leftmost_bit_tie_(uint32_t x)
{
    const uint64_t mask16 = 0x0000ffff0000ffffULL; // alternate half-words
    const uint64_t mask8  = 0x00ff00ff00ff00ffULL; // alternate bytes
    const uint64_t mask4  = 0x0f0f0f0f0f0f0f0fULL; // alternate nibbles
    const uint64_t mask2  = 0x3333333333333333ULL; // alternate 2-bits
    const uint64_t nibble_lsb = 0x1111111111111111ULL;
    const uint64_t nibble_msb = 0x8888888888888888ULL;
    uint64_t a, b, r, s, t, expx, pc_expx, nc_expx;
    int res;

    // common path can't handle all 0s and all 1s due to counter overflow
    if ((x == 0) || (x == ~0)) return 0;

    // make zero-nibble detection work, and simplify prefix sum computation
    x = (BitReverseTable256[x & 0xff] << 24) |
        (BitReverseTable256[(x >> 8) & 0xff] << 16) |
        (BitReverseTable256[(x >> 16) & 0xff] << 8) |
        (BitReverseTable256[(x >> 24) & 0xff]);; // bit reversal

    // expand each 2-bit into a nibble
    expx = x;
    expx = ((expx << 16) | expx) & mask16;
    expx = ((expx <<  8) | expx) & mask8;
    expx = ((expx <<  4) | expx) & mask4;
    expx = ((expx <<  2) | expx) & mask2;

    // compute positive and negative change counts for each nibble
    pc_expx = expx  & (expx >> 1) & nibble_lsb;
    nc_expx = ~expx & (~expx >> 1) & nibble_lsb;

    // produce prefix sums for positive and negative change counters
    a = pc_expx * nibble_lsb;
    b = nc_expx * nibble_lsb;

    // subtract positive and negative prefix sums, nibble-wise
    s = a ^ ~b;
    r = a | nibble_msb;
    t = b & ~nibble_msb;
    s = s & nibble_msb;
    r = r - t;
    r = r ^ s;

    // find first nibble that is zero using Alan Mycroft's magic
    r = (r - nibble_lsb) & (~r & nibble_msb);
    res = ffsll(r) / 2;  // account for 2-bit to nibble expansion

    return res;
}
*/
#endif //MOTIVO_PLATFORM_H
