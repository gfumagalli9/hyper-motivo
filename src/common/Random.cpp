//
// Created by steven on 3/11/17.
//
#include "Random.h"

template<> uint128_t Random::random_uint<uint128_t>(uint128_t from, uint128_t to_inclusive)
{
    uint128_t d = to_inclusive-from;
    uint64_t d1 = static_cast<uint64_t>(d>>64);
    uint64_t d2 = static_cast<uint64_t>(d & 0xFFFFFFFFFFFFFFFF);

    if(d1!=0)
    d1 = random_uint<uint64_t>(0, d1);

    if(d2!=0)
    d2 = random_uint<uint64_t>(0, d2);

    return from + ((static_cast<uint128_t>(d1)<<64) | d2);
}