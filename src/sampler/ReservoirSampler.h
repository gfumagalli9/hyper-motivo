//
// Created by steven on 11/27/16.
//

#ifndef MOTIVO_RESERVOIRSAMPLER_H
#define MOTIVO_RESERVOIRSAMPLER_H

#include "../common/Random.h"
#include "../platform.h"

template <typename T> class ReservoirSampler
{
private:
    unsigned long total_weight = 0;
    T current;
    Random* rnd;

public:
    inline ReservoirSampler(T default_value, Random* rnd) : current(default_value), rnd(rnd) {};
    inline T get_sample() const { return current; };

    //Let w_i be the weight of the i-th element and W_i be the total weight of the first i elements
    //After seeing num_elements elements the probability of returning the j-th element is
    //Pr(selecting j) * \Prod_{i=j+1}^num_elements (1 - Pr(not selecting i) )
    //= w_j / W_j * \Prod_{i=j+1}^num_elements (1 - w_i/W_i) = w_j / W_j * \Prod_{i=j+1}^num_elements (W_{i-1}/W_i)
    //= w_j / W_j  * W_j / W_n = w_j / W_n
    void feed(T element, uint64_t weight)
    {
        //total_weight += weight;
        add_overflow(total_weight, weight, &total_weight);
        if( weight!=0 && rnd->random_uint64(0, total_weight) < weight ) //probability of weight/total_weight = w_i / W_i
            current = element;
    }

};

#endif //MOTIVO_RESERVOIRSAMPLER_H
