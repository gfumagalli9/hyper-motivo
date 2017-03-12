//
// Created by steven on 11/13/16.
//

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
    Random(const std::string& seed="")
    {
        //Take into account 0 without overflowing
        static_assert( ((std::random_device::max()%256)+1)%256 == 0, "Random device range is not a multiple of 256");
        if(seed.empty())
        {
            std::random_device device;
            this->seed = "";
            for(int i=0; i<8; i++) //FIXME: We are wasting entropy
            {
                unsigned char r = static_cast<unsigned char>(device()%256);

                unsigned char c = static_cast<unsigned char>(r>>4);
                this->seed += static_cast<char>((c<10)?('0'+c):('A'+(c-10)));

                c = static_cast<unsigned char>(r & 0xF);
                this->seed += static_cast<char>((c<10)?('0'+c):('A'+(c-10)));
            }
        }
        else
            this->seed = seed;

        std::seed_seq seq(seed.begin(), seed.end());
        rng.seed(seq);
    }

    const std::string& get_seed() { return seed; }

    ///Returns an integer chosen uniformaly at random from @param from to @param to_inclusive
    template<typename T> T random_uint(T from, T to_inclusive)
    {
        static_assert(std::is_same<T, unsigned short>::value |
                      std::is_same<T, unsigned int>::value |
                      std::is_same<T, unsigned long>::value |
                      std::is_same<T, unsigned long long>::value, "Undefined behaviour according to the standard");

        std::uniform_int_distribution<T> uniform(from, to_inclusive);
        return uniform(rng);
    }
};

template<> uint128_t Random::random_uint<uint128_t>(uint128_t from, uint128_t to_inclusive);

#endif //MOTIVO_RANDOM_H