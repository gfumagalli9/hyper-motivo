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

#ifndef MOTIVO_RANDOM_H
#define MOTIVO_RANDOM_H

#include <cstdint>
#include <chrono>
#include <cassert>
#include <random>
#include <string>
#include "../platform/platform.h"


class Random
{
private:
    std::mt19937_64 rng;
    std::string seed;

public:
    explicit Random(const std::string& seed="")
    {
        //Take into account 0 without overflowing
        static_assert( ((std::random_device::max()%256)+1)%256 == 0, "Random device range is not a multiple of 256");
        if(seed.empty())
        {
            std::random_device device;
            this->seed = "";
            for(int i=0; i<8; i++) //FIXME: We are wasting entropy
            {
                auto r = static_cast<unsigned char>(device()%256);

                auto c = static_cast<unsigned char>(r>>4u);
                this->seed += static_cast<char>((c<10)?('0'+c):('A'+(c-10)));

                c = static_cast<unsigned char>(r & 0xFu);
                this->seed += static_cast<char>((c<10)?('0'+c):('A'+(c-10)));
            }
        }
        else
            this->seed = seed;

        std::seed_seq seq(this->seed.begin(), this->seed.end());
        rng.seed(seq);
    }


    const std::string& get_seed() const
    {
        return seed;
    }

    Random* derived_rng()
    {
        return new Random(std::to_string(random_uint<uint64_t>(0, std::numeric_limits<uint64_t>::max())));
    }

    ///Returns an integer chosen uniformly at random from @param from to @param to_inclusive
    template<typename T> T random_uint(T from, T to_inclusive)
    {
        static_assert(std::is_same<T, short>::value || std::is_same<T, unsigned short>::value ||
            std::is_same<T, int>::value || std::is_same<T, unsigned int>::value ||
            std::is_same<T, long>::value || std::is_same<T, unsigned long>::value ||
            std::is_same<T, long long>::value || std::is_same<T, unsigned long long>::value,
            "Undefined behaviour according to the standard");

        std::uniform_int_distribution<T> uniform(from, to_inclusive);
        return uniform(rng);
    }

    std::mt19937_64* underlying_generator()
    {
        return &rng;
    }
};

template<> inline uint128_t Random::random_uint<uint128_t>(uint128_t from, uint128_t to_inclusive)
{
    uint128_t d = to_inclusive-from;
    auto d1 = static_cast<uint64_t>(d>>64u);
    auto d2 = static_cast<uint64_t>(d & 0xFFFFFFFFFFFFFFFF);

    if(d1!=0)
        d1 = random_uint<uint64_t>(0, d1);

    if(d2!=0)
        d2 = random_uint<uint64_t>(0, d2);

    return from + ((static_cast<uint128_t>(d1)<<64u) | d2);
}

#endif //MOTIVO_RANDOM_H