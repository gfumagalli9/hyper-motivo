//
// Created by steven on 3/20/18.
//

#ifndef MOTIVO_RANGESAMPLER_H
#define MOTIVO_RANGESAMPLER_H

#include <cstdint>
#include <algorithm>
#include "Random.h"

template <typename T> class RangeSampler
{
private:

    struct range
    {
        T from;
        T to_exclusive;
    };

    range* ranges;
    uint64_t size;
    uint64_t capacity;

    T total_length;

public:
    RangeSampler() : size(0), capacity(2), total_length(0)
    {
        ranges = new range[capacity];
    }

    ~RangeSampler()
    {
        delete[] ranges;
    }

    T get_total_length() { return total_length; }

    void add_range(T from, T to_exclusive)
    {
        total_length += to_exclusive - from;

        if(size!=0 && ranges[size-1].to_exclusive==from)
        {
            ranges[size-1].to_exclusive = to_exclusive;
            return;
        }

        if(size==capacity)
        {
            capacity*=2;
            range* new_ranges = new range[capacity];
            std::copy(ranges, ranges+size, new_ranges);
            delete[] ranges;
            ranges = new_ranges;
        }

        ranges[size++] = {from, to_exclusive};

    }

    T sample(Random *rng)
    {
        assert(total_length>0);
        T rand = rng->random_uint<T>(0, total_length-1); //FIXME: Handle empty total length
        range* r=ranges;
        for(; rand >= r->to_exclusive - r->from; r++)
            rand -= r->to_exclusive - r->from;

        return r->from + rand;
    }
};


#endif //MOTIVO_RANGESAMPLER_H
