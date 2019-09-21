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

//
// Created by steven on 12/2/16.
//

#ifndef MOTIVO_PLATFORM_H
#define MOTIVO_PLATFORM_H

#include <cstdlib>
#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <memory.h>
#include <limits>
#include <sys/mman.h>
#include "leftmost_bit_tie_lut.h"
#include "config.h"

#define FAIL_OVERFLOW do { std::cerr << "Overflow in " << __FILE__ <<":"<< __LINE__ << std::endl << std::flush; std::abort(); } while(false)

#ifdef MOTIVO_HAS_BUILTIN_ADD_OVERFLOW
    #define add_overflow(a, b, res) __builtin_add_overflow( (a), (b), (res) )
#else
    template<typename T> inline bool add_overflow(T a, T b, T* res)
    {
        if( (b>0 && a>std::numeric_limits<T>::max()-b) || (b<0 && a < std::numeric_limits<T>::min()-b) )
            return true;

        *res=a+b;
        return false;
    }
#endif

#ifdef MOTIVO_HAS_BUILTIN_MUL_OVERFLOW
    #define mul_overflow(a, b, res) __builtin_mul_overflow( (a), (b), (res) )
#else
    template<typename T> typename std::enable_if<std::is_unsigned<T>::value, bool>::type inline mul_overflow(T a, T b, T* res)
    {
        if( (a>std::numeric_limits<T>::max()/b) || (a < std::numeric_limits<T>::min()/b) )
            return true;

        *res=a*b;
        return false;
    }

    template<typename T> typename std::enable_if<!std::is_unsigned<T>::value, bool>::type inline mul_overflow(T a, T b, T* res)
    {
        if( (a==-1 && b==std::numeric_limits<T>::min())  || (b==-1 && a==std::numeric_limits<T>::min())
            || (a>std::numeric_limits<T>::max()/b) || (a < std::numeric_limits<T>::min()/b) )
            return true;

        *res=a*b;
        return false;
    }
#endif

#ifdef MOTIVO_OVERFLOW_SAFE
    #define safe_add(a, b, res)  do { if( add_overflow( (a), (b), (res) ) ) { FAIL_OVERFLOW; } } while(false)
    #define safe_mul(a, b, res)  do { if( mul_overflow( (a), (b), (res) ) ) { FAIL_OVERFLOW; } } while(false)
#else
    #define safe_add(a, b, res) do { (*res) = ( (a) + (b) ); } while(false)
    #define safe_mul(a, b, res) do { (*res) = ( (a) * (b) ); } while(false)
#endif

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

#ifndef MOTIVO_HAS_UINT128_T
#ifdef MOTIVO_HAS___UINT128_T
    typedef __uint128_t uint128_t;
#else
    #error "No 128-bit unsigned integer type"
#endif
#endif

///@pre the leftmost bit of x is 1
///@returns the index of the smallest index i>0 such that the number of 0s and 1s in the leftmost i bits of x are equal
inline uint8_t leftmost_bit_tie1 [[gnu::pure]] (uint32_t x)
{
    uint8_t y = leftmost_bit_tie_LUT0[ (x>>24u) & 0b01111111u];
    if(y & 0b10000000u)
        return static_cast<uint8_t>(~y);

    y = leftmost_bit_tie_LUT1[ static_cast<unsigned int>(y<<8u) | ((x>>16u) & 0xFFu) ];
    if(y & 0b10000000u)
        return static_cast<uint8_t>(~y);

    y = leftmost_bit_tie_LUT2[ static_cast<unsigned int>(y<<8u) | ((x>>8u) & 0xFFu) ];
    if(y & 0b10000000u)
        return static_cast<uint8_t>(~y);

    y = leftmost_bit_tie_LUT3[static_cast<unsigned int>(y<<8u) | (x & 0xFFu) ];
    return static_cast<uint8_t>(~y);
}

///@returns the index of the smallest index i>0 such that the number of 0s and 1s in the leftmost i bits of x are equal
inline int leftmost_bit_tie [[gnu::pure, gnu::flatten]] (uint32_t x) { return leftmost_bit_tie1((x>>31u)?x:~x); }

///wraps mmap
void* motivo_mmap_populate(size_t length, int prot, int fd);
void* motivo_mmap(size_t length, int prot, int fd);
void motivo_prefault(off_t off, size_t length, int fd);

//wraps munmap
int motivo_munmap(void* addr, size_t length);

#endif //MOTIVO_PLATFORM_H
