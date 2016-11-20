//
// Created by steven on 11/18/16.
//

#include "doctest.h"

#include "../src/treelet.h"

uint64_t pack(uint32_t structure, uint16_t colors, uint8_t children, uint8_t size)
{
    uint64_t r;
    uint8_t* pr= reinterpret_cast<uint8_t*>(&r);

    *reinterpret_cast<uint32_t*>(pr) = structure;
    *reinterpret_cast<uint16_t*>(pr+4) = colors;
    *reinterpret_cast<uint8_t*>(pr+6) = children;
    *reinterpret_cast<uint8_t*>(pr+7) = size;

    return r;
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
    Treelet::treelet_t t0 = Treelet::singleton(0);
    Treelet::treelet_t t1 = Treelet::singleton(1);
    Treelet::treelet_t t2 = Treelet::singleton(2);
    Treelet::treelet_t t3 = Treelet::singleton(3);
    Treelet::treelet_t t4 = Treelet::singleton(4);
    Treelet::treelet_t t5 = Treelet::singleton(5);
    Treelet::treelet_t t6 = Treelet::singleton(6);

    CHECK(Treelet::merge(t0, t0) == Treelet::invalid_treelet);

    //A path 0--1
    Treelet::treelet_t t0_1 = Treelet::merge(t0, t1);
    CHECK(t0_1 == pack(0b10000000000000000000000000000000, 0b0000000000000011, 1, 2));

    CHECK(Treelet::merge(t0_1, t0) == Treelet::invalid_treelet);
    CHECK(Treelet::merge(t0_1, t0) == Treelet::invalid_treelet);
    CHECK(Treelet::merge(t0, t0_1) == Treelet::invalid_treelet);
    CHECK(Treelet::merge(t1, t0_1) == Treelet::invalid_treelet);

    //A star 0--1, 0--2
    Treelet::treelet_t t0_12 = Treelet::merge(t0_1, t2);
    CHECK(t0_12 == pack(0b10100000000000000000000000000000, 0b0000000000000111, 2, 3));
    CHECK(Treelet::merge(t0_12, t0) == Treelet::invalid_treelet);
    CHECK(Treelet::merge(t0_12, t1) == Treelet::invalid_treelet);
    CHECK(Treelet::merge(t0_12, t0_1) == Treelet::invalid_treelet);

    //A star 0--1, 0--2, 0--3
    Treelet::treelet_t t0_123 = Treelet::merge(t0_12, t3);
    CHECK(t0_123 == pack(0b10101000000000000000000000000000, 0b0000000000001111, 3, 4));

    //A path 3--4
    Treelet::treelet_t t3_4 = Treelet::merge(t3, t4);
    CHECK(t3_4 == pack(0b10000000000000000000000000000000, 0b0000000000011000, 1, 2));

    //A star with an extra leaf, 0--1, 0--2, 0--3, 3--4
    Treelet::treelet_t t0_123_4 = Treelet::merge(t0_12, t3_4);
    CHECK(t0_123_4 == pack(0b11001010000000000000000000000000, 0b0000000000011111, 1, 5));

    //A path 2--3, 3--4
    Treelet::treelet_t t2_3_4 = Treelet::merge(t2, t3_4);
    CHECK(t2_3_4 == pack(0b11000000000000000000000000000000, 0b0000000000011100, 1, 3));

    //A spider of height 2 with two legs 2--3, 3--4, 2--0, 0--1
    Treelet::treelet_t t2_3_4__0_1 = Treelet::merge(t2_3_4, t0_1);
    CHECK(t2_3_4__0_1 == pack(0b11001100000000000000000000000000, 0b0000000000011111, 2, 5));

    //Invalid because we are merging with a "smaller" children
    Treelet::treelet_t t2_3_4__0_1__5 = Treelet::merge(t2_3_4__0_1, t5);
    CHECK(t2_3_4__0_1__5 == Treelet::invalid_treelet);

    //A path 5--6
    Treelet::treelet_t t5_6 = Treelet::merge(t5, t6);
    CHECK(t5_6 == pack(0b10000000000000000000000000000000, 0b0000000001100000, 1, 2));

    //A spider of height 2 with three legs 2--3, 3--4, 2--0, 0--1, 2--5, 5--6
    Treelet::treelet_t t2_3_4__0_1__5_6 = Treelet::merge(t2_3_4__0_1, t5_6);
    CHECK(t2_3_4__0_1__5_6== pack(0b11001100110000000000000000000000, 0b0000000001111111, 3, 7));

    //A path 5--2, 2--3, 3--4
    Treelet::treelet_t t5_2_3_4 = Treelet::merge(t5, t2_3_4);
    CHECK(t5_2_3_4 == pack(0b11100000000000000000000000000000, 0b0000000000111100, 1, 4));

    //Invalid because we are merging with a "smaller" children
    Treelet::treelet_t t5_2_3_4__0_1 = Treelet::merge(t5_2_3_4, t0_1);
    CHECK(t5_2_3_4__0_1 == Treelet::invalid_treelet);

    //A star 0--1, 0--6
    Treelet::treelet_t t0_16 = Treelet::merge(t0_1, t6);
    CHECK(t0_16 == pack(0b10100000000000000000000000000000, 0b0000000001000011, 2, 3));

    //Invalid because we are merging with a "smaller" children
    Treelet::treelet_t t5_2_3_4__0_16 = Treelet::merge(t5_2_3_4, t0_16);
    CHECK(t5_2_3_4__0_16 == Treelet::invalid_treelet);
}