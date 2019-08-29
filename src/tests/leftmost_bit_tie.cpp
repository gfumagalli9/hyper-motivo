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

#include "doctest.h"
#include "../common/platform/platform.h"

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

        x = x << 1u;
    } while (count && bits<32);

    return count?127:bits;
}

TEST_CASE("leftmost_bit_tie slow")
{
    CHECK(reference(0)==leftmost_bit_tie(0));

    uint32_t x=1;
    while(x && reference(x)==leftmost_bit_tie(x))
        x++;

    CHECK(x==0);
}