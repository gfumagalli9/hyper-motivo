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

#ifndef MOTIVO_CONCURRENTFIFO_H
#define MOTIVO_CONCURRENTFIFO_H

#include <mutex>
#include <condition_variable>

template <typename T> class ConcurrentFIFO
{
private:
    T* buffer;
    const unsigned long capacity;
    unsigned long head = 0;
    unsigned long size = 0;

    std::mutex mutex;
    std::condition_variable not_full;
    std::condition_variable not_empty;

public:
    explicit ConcurrentFIFO(unsigned long capacity) : capacity(capacity)
    {
        buffer = new T[capacity];
    }

    ~ConcurrentFIFO()
    {
        delete[] buffer;
    }

    unsigned long get_size()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return size;
    }

    void push(T element)
    {
        std::unique_lock<std::mutex> lock(mutex);

        not_full.wait(lock, [this]{ return size!=capacity; } );

        buffer[ (head+size)%capacity ] = element;
        size++;

        //if(size==1)
        //{
            lock.unlock();
            not_empty.notify_one();
        //}
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(mutex);

        not_empty.wait(lock, [this]{ return size>0; } );
        T element = buffer[head];
        head = (head+1)%capacity;
        size--;

        lock.unlock();
        not_full.notify_one();

        return element;
    }

};


#endif //MOTIVO_CONCURRENTFIFO_H
