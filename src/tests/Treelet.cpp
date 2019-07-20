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
#include "../common/treelets/Treelet.h"

void test(const Treelet& treelet, Treelet::treelet_structure_t structure, Treelet::treelet_colors_t colors, uint8_t norm, uint8_t size)
{
    struct [[gnu::packed]]
    {
        Treelet::treelet_structure_t s;
        Treelet::treelet_colors_t c;
    } r{structure, colors};

    static_assert(sizeof(r) == sizeof(Treelet), "Structure size mismatch");
    CHECK( memcmp(&r, &treelet, sizeof(r)) == 0 );
    CHECK(treelet.number_of_vertices()==size);
    CHECK(treelet.normalization_factor()==norm);
}

TEST_CASE("Treelet signletons")
{
    for(uint8_t i=0; i<16; i++)
    {
        test(Treelet::singleton(i), 0b00000000000000000000000000000000, static_cast<uint16_t>(1<<i), 1, 1);
    }
}

TEST_CASE("Treelet merges")
{
    Treelet t0 = Treelet::singleton(0);
    Treelet t1 = Treelet::singleton(1);
    Treelet t2 = Treelet::singleton(2);
    Treelet t3 = Treelet::singleton(3);
    Treelet t4 = Treelet::singleton(4);
    Treelet t5 = Treelet::singleton(5);
    Treelet t6 = Treelet::singleton(6);

    CHECK(t0.merge(t0) == invalid_merge_colors);

    //A path 0--1
    Treelet t0_1 = t0.merge(t1);
    test(t0_1, 0b10000000000000000000000000000000, 0b0000000000000011, 1, 2);

    CHECK(t0_1.merge(t0) == invalid_merge_colors);
    CHECK(t0_1.merge(t1) == invalid_merge_colors);
    CHECK(t0.merge(t0_1) == invalid_merge_colors);
    CHECK(t1.merge(t0_1) == invalid_merge_colors);

    //A star 0--1, 0--2
    Treelet t0_12 = t0_1.merge(t2);
    test(t0_12, 0b10100000000000000000000000000000, 0b0000000000000111, 2, 3);
    CHECK(t0_12.merge(t0) == invalid_merge_colors);
    CHECK(t0_12.merge(t1) == invalid_merge_colors);
    CHECK(t0_12.merge(t0_1) == invalid_merge_colors);

    //A star 0--1, 0--2, 0--3
    Treelet t0_123 = t0_12.merge(t3);
    test(t0_123, 0b10101000000000000000000000000000, 0b0000000000001111, 3, 4);

    //A path 3--4
    Treelet t3_4 = t3.merge(t4);
    test(t3_4, 0b10000000000000000000000000000000, 0b0000000000011000, 1, 2);

    //A star with an extra leaf, 0--1, 0--2, 0--3, 3--4
    Treelet t0_123_4 = t0_12.merge(t3_4);
    test(t0_123_4, 0b11001010000000000000000000000000, 0b0000000000011111, 1, 5);

    //A path 2--3, 3--4
    Treelet t2_3_4 = t2.merge(t3_4);
    test(t2_3_4, 0b11000000000000000000000000000000, 0b0000000000011100, 1, 3);

    //A spider of height 2 with two legs 2--3, 3--4, 2--0, 0--1
    Treelet t2_3_4__0_1 = t2_3_4.merge(t0_1);
    test(t2_3_4__0_1, 0b11001100000000000000000000000000, 0b0000000000011111, 2, 5);

    //Invalid because we are merging with a "smaller" child
    Treelet t2_3_4__0_1__5 = t2_3_4__0_1.merge(t5);
    CHECK(t2_3_4__0_1__5 == invalid_merge_structure);

    //A path 5--6
    Treelet t5_6 = t5.merge(t6);
    test(t5_6, 0b10000000000000000000000000000000, 0b0000000001100000, 1, 2);

    //A spider of height 2 with three legs 2--3, 3--4, 2--0, 0--1, 2--5, 5--6
    Treelet t2_3_4__0_1__5_6 = t2_3_4__0_1.merge(t5_6);
    test(t2_3_4__0_1__5_6, 0b11001100110000000000000000000000, 0b0000000001111111, 3, 7);
    CHECK(t2_3_4__0_1__5_6.split_child().get_structure() == t5_6.get_structure());
    CHECK(t2_3_4__0_1__5_6.complement(t5_6) == t2_3_4__0_1);

    //A path 5--2, 2--3, 3--4
    Treelet t5_2_3_4 = t5.merge(t2_3_4);
    test(t5_2_3_4, 0b11100000000000000000000000000000, 0b0000000000111100, 1, 4);

    //Invalid because we are merging with a "smaller" child
    Treelet t5_2_3_4__0_1 = t5_2_3_4.merge(t0_1);
    CHECK(t5_2_3_4__0_1 == invalid_merge_structure);

    //A star 0--1, 0--6
    Treelet t0_16 = t0_1.merge(t6);
    test(t0_16, 0b10100000000000000000000000000000, 0b0000000001000011, 2, 3);

    //Invalid because we are merging with a "smaller" child
    Treelet t5_2_3_4__0_16 = t5_2_3_4.merge(t0_16);
    CHECK(t5_2_3_4__0_16 == invalid_merge_structure);
}