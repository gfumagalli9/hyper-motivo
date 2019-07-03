//
// Created by steven on 2/22/19.
//

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
