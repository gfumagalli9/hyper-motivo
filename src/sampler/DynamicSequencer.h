//
// Created by steven on 1/12/19.
//

#ifndef MOTIVO_DYNAMICSEQUENCER_H
#define MOTIVO_DYNAMICSEQUENCER_H

#include <mutex>
#include <limits>

template <typename T> class DynamicSequencer
{
public:
    static constexpr T to_max = std::numeric_limits<T>::max() - 2*(std::numeric_limits<T>::max()/100);

    struct sequence_batch_t
    {
        T from;
        T to;
    };

private:
    T next;
    const T end;

    const unsigned int nthreads;
    std::mutex mutex;

public:
    DynamicSequencer(T start, T end, unsigned int nthreads) : next(start), end(end+1), nthreads(nthreads)
    {}

    sequence_batch_t next_batch()
    {
        sequence_batch_t batch;
        T step;

        mutex.lock();
        batch.from = next;

        if(next<end)
        {
            step = (end-next)/(nthreads*100);
            if(step<1)
                step=1;

            next+=step;
        }
        mutex.unlock();

        batch.to = batch.from + step;
        if(batch.to > end)
            batch.to = end;

        return batch;
    }
};


#endif //MOTIVO_DYNAMICSEQUENCER_H
