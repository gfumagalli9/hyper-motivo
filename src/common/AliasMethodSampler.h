//
// Created by steven on 12/6/16.
//

#ifndef MOTIVO_ALIASMETHODSAMPLER_H
#define MOTIVO_ALIASMETHODSAMPLER_H


#include <cstdio>
#include <cstdint>
#include <iostream>
#include "Random.h"

class AliasMethodSampler
{
private:
    struct element
    {
        uint64_t U;
        uint64_t K;
    };

    static_assert(sizeof(element) == 16, "struct element is not packed");

    uint64_t num_elements;
    uint64_t total_weight;
    element* elements;
    FILE* elements_fd;
    bool readonly;

    AliasMethodSampler(const AliasMethodSampler&) = delete;
    void operator=(const AliasMethodSampler&) = delete;

public:
    AliasMethodSampler(const std::string& filename);
    AliasMethodSampler(uint64_t n);
    ~AliasMethodSampler();

    void set(const uint64_t n, const uint64_t weight);

    void build();

    inline uint64_t sample(Random* rng)
    {
        assert(readonly);
        assert(total_weight>0);

        uint64_t i = rng->random_uint64(0, num_elements);
        uint64_t y = rng->random_uint64(0, total_weight);

        assert(elements[i].U==total_weight || elements[i].K<num_elements);

        return (y<elements[i].U)?i:elements[i].K;
    };

    bool write(const std::string& filename);
};


#endif //MOTIVO_ALIASMETHODSAMPLER_H
