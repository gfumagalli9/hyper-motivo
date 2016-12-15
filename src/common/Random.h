//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_RANDOM_H
#define MOTIVO_RANDOM_H

#include <cstdint>
#include <chrono>
#include <cassert>
#include <random>

class Random
{
private:
    std::mt19937_64 rng;

public:
    Random()
    {
        std::random_device device;
        std::seed_seq seq{device(), device(), device(), device()};
        rng.seed(seq);
    }

    ///Returns an integer chosen uniformaly at random from @param from to @param to_exclusive - 1
    uint64_t random_uint64(uint64_t from, uint64_t to_exclusive)
    {
        static_assert(std::is_same<uint64_t, unsigned short>::value |
                      std::is_same<uint64_t, unsigned int>::value |
                      std::is_same<uint64_t, unsigned long>::value |
                      std::is_same<uint64_t, unsigned long long>::value, "Undefined behaviour according to the standard");

        std::uniform_int_distribution<uint64_t> uniform(from, to_exclusive-1);
        return uniform(rng);
    }

    uint64_t random_uint32(uint32_t from, uint32_t to_exclusive)
    {
        static_assert(std::is_same<uint32_t, unsigned short>::value |
                      std::is_same<uint32_t, unsigned int>::value |
                      std::is_same<uint32_t, unsigned long>::value |
                      std::is_same<uint32_t, unsigned long long>::value, "Undefined behaviour according to the standard");

        std::uniform_int_distribution<uint32_t> uniform(from, to_exclusive-1);
        return uniform(rng);
    }
};

#endif //MOTIVO_RANDOM_H