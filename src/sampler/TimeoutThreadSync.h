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

#ifndef MOTIVO_TIMEOUTTHREADSYNC_H
#define MOTIVO_TIMEOUTTHREADSYNC_H

#include <mutex>
#include <atomic>
#include <condition_variable>

class TimeoutThreadSync
{
private:
    const unsigned int number_of_threads;
    unsigned int terminated_threads = 0;
    std::mutex mutex;
    std::condition_variable all_threads_terminated;

    std::atomic<bool>* termination_flags;

    static_assert(std::atomic<bool>::is_always_lock_free, "std::atomic<bool> is not always lock-free");

public:
    explicit TimeoutThreadSync(unsigned int nthreads) : number_of_threads(nthreads)
    {
        termination_flags = new std::atomic<bool>[number_of_threads]();
    }

    TimeoutThreadSync(TimeoutThreadSync&) = delete; //Deleted copy constructor

    ~TimeoutThreadSync()
    {
        delete[] termination_flags;
    }

    std::atomic<bool>& get_termination_flag(unsigned int thread_no)
    {
        assert(thread_no < number_of_threads);

        return termination_flags[thread_no];
    }

    void signal_termination_one()
    {
        std::unique_lock lock(mutex);
        if(++terminated_threads==number_of_threads)
            all_threads_terminated.notify_one();
    }

    void request_termination()
    {
        for(unsigned int i=0; i<number_of_threads; i++)
            termination_flags[i] = true;
    }

    void wait_timeout(double timeout_seconds)
    {
        std::unique_lock lock(mutex);
        all_threads_terminated.wait_for(lock, std::chrono::duration<double>(timeout_seconds), [this] {return terminated_threads==number_of_threads; } );
    }

    void wait()
    {
        std::unique_lock lock(mutex);
        all_threads_terminated.wait(lock, [this] {return terminated_threads==number_of_threads; } );
    }
};

#endif //MOTIVO_TIMEOUTTHREADSYNC_H
