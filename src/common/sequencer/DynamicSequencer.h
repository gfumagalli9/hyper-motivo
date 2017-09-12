//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_DYNAMICSEQUENCER_H
#define MOTIVO_DYNAMICSEQUENCER_H

#include <cstdint>
#include <mutex>
#include "BaseSequencer.h"


template <typename T> class DynamicSequencer : public BaseSequencer<T>
{
public:
    typedef void (*progress_callback_t)(T);

private:
    T next;
    const T end;
    const unsigned int nthreads;

    std::mutex mutex;

    progress_callback_t progress_callback;
    T progress_interval;

public:
    DynamicSequencer(T start, T end, unsigned int nthreads) : next(start), end(end+1), nthreads(nthreads), progress_callback(nullptr) {}

    typename BaseSequencer<T>::sequence_batch_t next_batch()
    {
        typename BaseSequencer<T>::sequence_batch_t batch;
        T step=0;

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

        if(progress_callback!=nullptr && (batch.from/progress_interval)<(batch.to/progress_interval))
            (*progress_callback)(batch.from);

        return batch;
    }

    void set_progress_callback(progress_callback_t callback, T interval)
    {
        progress_callback = callback;
        progress_interval = interval;
    }
};


#endif //MOTIVO_DYNAMICSEQUENCER_H
