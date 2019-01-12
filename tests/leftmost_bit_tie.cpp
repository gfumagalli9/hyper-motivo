//
// Created by steven on 12/3/16.
//

#include "doctest.h"
#include "../src/common/platform/platform.h"

int reference (uint32_t x)
{
    int count = 0;
    int bits = 0;
    uint32_t mask = 0x80000000;
    do {
        bits++;

        if (x & mask)
            count++;
        else
            count--;

        x = x << 1;
    } while (count && bits<32);

    return count?127:bits;
}

TEST_CASE("leftmost_bit_tie")
{
    CHECK(reference(0)==leftmost_bit_tie(0));

    uint32_t x=1;
    while(x && reference(x)==leftmost_bit_tie(x))
        x++;

    CHECK(x==0);
}