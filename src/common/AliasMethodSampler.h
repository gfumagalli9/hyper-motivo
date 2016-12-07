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

public:
    AliasMethodSampler(const std::string& filename);
    AliasMethodSampler(uint64_t n);
    ~AliasMethodSampler();

    void set(const uint64_t n, const uint64_t weight);

    void build();

    inline uint64_t sample(Random* rng)
    {
        assert(readonly);
        //uint64_t r = rng->random_uint64(0, num_elements*total_weight);
        /*uint64_t i = r/total_weight; //i is an uniform between 0 and num_elements-1
        uint64_t y = r%total_weight; //y is an uniform between 0 and total_weight-1
        if(y<U[i])
            return i;
        return K[i];*/

        //return (r%total_weight<U[r/total_weight])?(r/total_weight):K[r/total_weight];

        uint64_t i = rng->random_uint64(0, num_elements);
        uint64_t y = rng->random_uint64(0, total_weight);
        //std::cout << i << " " << y << " " << elements[i].U << " " << elements[i].K << std::endl << std::flush;
        return (y<elements[i].U)?i:elements[i].K;
    };

    bool write(const std::string& filename);
};


#endif //MOTIVO_ALIASMETHODSAMPLER_H
