//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_STATICSEQUENCER_H
#define MOTIVO_STATICSEQUENCER_H

#include <atomic>
#include "BaseSequencer.h"

template <typename T> class StaticSequencer : public BaseSequencer<T>
{
public:
    typedef void (*progress_callback_t)(T);

private:
    std::atomic<T> next;
    const T end;
    const T batch_size;
    progress_callback_t progress_callback;
    T progress_interval;

public:
    StaticSequencer(T start, T end, T batch_size)
            : next(start), end(end+1), batch_size(batch_size), progress_callback(nullptr) {}

    typename BaseSequencer<T>::sequence_batch_t next_batch()
    {
        typename BaseSequencer<T>::sequence_batch_t batch;
        batch.from = next.fetch_add(batch_size);
        batch.to = batch.from + batch_size;

        if (batch.to>end)
            batch.to=end;

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


#endif //MOTIVO_STATICSEQUENCER_H
