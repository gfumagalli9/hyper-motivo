// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
