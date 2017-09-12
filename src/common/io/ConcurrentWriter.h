//
// Created by steven on 9/11/17.
//

#ifndef MOTIVO_CONCURRENTWRITER_H
#define MOTIVO_CONCURRENTWRITER_H


#include <cstdio>
#include <iostream>
#include <thread>
#include "ConcurrentFIFO.h"

class ConcurrentWriter
{
private:
    struct record_t
    {
        char* buffer;
        std::size_t length;
    };

    std::ostream* output;
    ConcurrentFIFO<record_t> queue;
    std::thread write_thread;
    bool closed;

    void write_loop();

public:
    ConcurrentWriter(std::ostream* output, unsigned long capacity);
    ~ConcurrentWriter() { close(); }

    void write(char* buffer, std::size_t length)  { queue.push( {buffer, length} ); }

    void close();
};


#endif //MOTIVO_CONCURRENTWRITER_H
