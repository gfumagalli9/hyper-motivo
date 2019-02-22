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
        T to_exclusive;
    };

private:
    T next;
    const T end_exclusive;

    const unsigned int nthreads;
    std::mutex mutex;

public:
    DynamicSequencer(T start, T end_exclusive, unsigned int nthreads) : next(start), end_exclusive(end_exclusive), nthreads(nthreads)
    {}

    sequence_batch_t next_batch()
    {
        sequence_batch_t batch;

        mutex.lock();
        batch.from = next;

        if(next>=end_exclusive)
        {
            mutex.unlock();
            batch.to_exclusive=end_exclusive;
            return batch;
        }

        T step = (end_exclusive-next)/(nthreads*100);
        if(step<1)
            step=1;

        next+=step;
        mutex.unlock();

        batch.to_exclusive = batch.from + step;
        if(batch.to_exclusive > end_exclusive)
            batch.to_exclusive = end_exclusive;

        return batch;
    }
};


#endif //MOTIVO_DYNAMICSEQUENCER_H
