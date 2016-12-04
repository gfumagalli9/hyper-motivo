//
// Created by steven on 11/18/16.
//

#include "doctest.h"
#include "../src/Treelet.h"
#include "../src/bit_cast.h"

Treelet pack(uint32_t structure, uint16_t colors, uint8_t children, uint8_t size)
{
    uint8_t r[6];

    *reinterpret_cast<uint32_t*>(r) = structure;
    *reinterpret_cast<uint16_t*>(r+4) = colors;
    //*reinterpret_cast<uint8_t*>(pr+6) = children;
    //*reinterpret_cast<uint8_t*>(pr+7) = size;

//    REQUIRE( r==Treelet::treelet_pack(structure, colors, children, size) );

    return bit_cast<Treelet>(r);
}

TEST_CASE("treelet signletons")
{
    CHECK(Treelet::singleton(0)  == pack(0b00000000000000000000000000000000, 0b0000000000000001, 0, 1));
    CHECK(Treelet::singleton(1)  == pack(0b00000000000000000000000000000000, 0b0000000000000010, 0, 1));
    CHECK(Treelet::singleton(2)  == pack(0b00000000000000000000000000000000, 0b0000000000000100, 0, 1));
    CHECK(Treelet::singleton(15) == pack(0b00000000000000000000000000000000, 0b1000000000000000, 0, 1));
}

TEST_CASE("treelet merges")
{
    Treelet t0 = Treelet::singleton(0);
    Treelet t1 = Treelet::singleton(1);
    Treelet t2 = Treelet::singleton(2);
    Treelet t3 = Treelet::singleton(3);
    Treelet t4 = Treelet::singleton(4);
    Treelet t5 = Treelet::singleton(5);
    Treelet t6 = Treelet::singleton(6);

    CHECK(t0.merge(t0) == Treelet::invalid_merge_colors);

    //A path 0--1
    Treelet t0_1 = t0.merge(t1);
    CHECK(t0_1 == pack(0b10000000000000000000000000000000, 0b0000000000000011, 1, 2));

    CHECK(t0_1.merge(t0) == Treelet::invalid_merge_colors);
    CHECK(t0_1.merge(t1) == Treelet::invalid_merge_colors);
    CHECK(t0.merge(t0_1) == Treelet::invalid_merge_colors);
    CHECK(t1.merge(t0_1) == Treelet::invalid_merge_colors);

    //A star 0--1, 0--2
    Treelet t0_12 = t0_1.merge(t2);
    CHECK(t0_12 == pack(0b10100000000000000000000000000000, 0b0000000000000111, 2, 3));
    CHECK(t0_12.merge(t0) == Treelet::invalid_merge_colors);
    CHECK(t0_12.merge(t1) == Treelet::invalid_merge_colors);
    CHECK(t0_12.merge(t0_1) == Treelet::invalid_merge_colors);

    //A star 0--1, 0--2, 0--3
    Treelet t0_123 = t0_12.merge(t3);
    CHECK(t0_123 == pack(0b10101000000000000000000000000000, 0b0000000000001111, 3, 4));

    //A path 3--4
    Treelet t3_4 = t3.merge(t4);
    CHECK(t3_4 == pack(0b10000000000000000000000000000000, 0b0000000000011000, 1, 2));

    //A star with an extra leaf, 0--1, 0--2, 0--3, 3--4
    Treelet t0_123_4 = t0_12.merge(t3_4);
    CHECK(t0_123_4 == pack(0b11001010000000000000000000000000, 0b0000000000011111, 1, 5));

    //A path 2--3, 3--4
    Treelet t2_3_4 = t2.merge(t3_4);
    CHECK(t2_3_4 == pack(0b11000000000000000000000000000000, 0b0000000000011100, 1, 3));

    //A spider of height 2 with two legs 2--3, 3--4, 2--0, 0--1
    Treelet t2_3_4__0_1 = t2_3_4.merge(t0_1);
    CHECK(t2_3_4__0_1 == pack(0b11001100000000000000000000000000, 0b0000000000011111, 2, 5));

    //Invalid because we are merging with a "smaller" child
    Treelet t2_3_4__0_1__5 = t2_3_4__0_1.merge(t5);
    CHECK(t2_3_4__0_1__5 == Treelet::invalid_merge_structure);

    //A path 5--6
    Treelet t5_6 = t5.merge(t6);
    CHECK(t5_6 == pack(0b10000000000000000000000000000000, 0b0000000001100000, 1, 2));

    //A spider of height 2 with three legs 2--3, 3--4, 2--0, 0--1, 2--5, 5--6
    Treelet t2_3_4__0_1__5_6 = t2_3_4__0_1.merge(t5_6);
    CHECK(t2_3_4__0_1__5_6== pack(0b11001100110000000000000000000000, 0b0000000001111111, 3, 7));
    CHECK(t2_3_4__0_1__5_6.split_child().get_structure() == t5_6.get_structure());
    CHECK(t2_3_4__0_1__5_6.complement(t5_6) == t2_3_4__0_1);

    //A path 5--2, 2--3, 3--4
    Treelet t5_2_3_4 = t5.merge(t2_3_4);
    CHECK(t5_2_3_4 == pack(0b11100000000000000000000000000000, 0b0000000000111100, 1, 4));

    //Invalid because we are merging with a "smaller" child
    Treelet t5_2_3_4__0_1 = t5_2_3_4.merge(t0_1);
    CHECK(t5_2_3_4__0_1 == Treelet::invalid_merge_structure);

    //A star 0--1, 0--6
    Treelet t0_16 = t0_1.merge(t6);
    CHECK(t0_16 == pack(0b10100000000000000000000000000000, 0b0000000001000011, 2, 3));

    //Invalid because we are merging with a "smaller" child
    Treelet t5_2_3_4__0_16 = t5_2_3_4.merge(t0_16);
    CHECK(t5_2_3_4__0_16 == Treelet::invalid_merge_structure);
}