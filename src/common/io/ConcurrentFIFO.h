//
// Created by steven on 3/1/17.
//

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
    ConcurrentFIFO(unsigned long capacity) : capacity(capacity)
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
