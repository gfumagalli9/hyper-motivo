//
// Created by steven on 11/13/16.
//

#ifndef MOTIVO_RANDOM_H
#define MOTIVO_RANDOM_H

#include <cstdint>
#include <gsl/gsl_rng.h>
#include <chrono>

class Random
{
private:
    gsl_rng *rng;

public:
    Random()
    {
        rng = gsl_rng_alloc(gsl_rng_default);

        auto now = std::chrono::high_resolution_clock::now();
        gsl_rng_set(rng, (unsigned long)now.time_since_epoch().count());
    }

    ~Random() {  gsl_rng_free(rng); }

    ///Returns an integer chosen uniformaly at random from @param from to @param to_exclusive - 1
    ///@param < @param to_exclusive  and their difference must be less than the 2147483647.
    int32_t random_int32(int32_t from, int32_t to_exclusive)
    {
        return from + (int32_t)gsl_rng_uniform_int(rng, (unsigned long)(to_exclusive-from));
    }
};

#endif //MOTIVO_RANDOM_H