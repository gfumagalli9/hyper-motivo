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
