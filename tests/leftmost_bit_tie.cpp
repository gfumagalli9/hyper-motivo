//
// Created by steven on 12/3/16.
//

#include "doctest.h"
#include "../src/platform.h"

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
    bool success=true;
    uint32_t x=0;
    do
    {
        if(reference(x)!=leftmost_bit_tie(x))
        {
            success=false;
            break;
        }
    } while(++x);

    CHECK(success);
}