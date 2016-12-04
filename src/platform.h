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
#include "config.h"


///popcount32 returns the number of bits set to 1 in x where x is a 32 bit integer
#if MOTIVO_INT_SIZE>=4 && MOTIVO_HAS_BUILTIN_POPCOUNT
    #define popcount32(x) ( __builtin_popcount( (x) ) )
#elif MOTIVO_LONG_SIZE>=4 && MOTIVO_HAS_BUILTIN_POPCOUNTL
    #define popcount32(x) ( __builtin_popcountl( (x) ) )
#else
//From: https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
inline int popcount32 [[gnu::const]] (uint32_t v)
{
    v = v - ((v >> 1) & 0x55555555);                    // reuse input as temporary
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);     // temp
    return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; // count
}
#endif


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

#endif //MOTIVO_PLATFORM_H
