//
// Created by steven on 9/11/17.
//

#include "ConcurrentWriter.h"

ConcurrentWriter::ConcurrentWriter(std::ostream *output, unsigned long capacity) : output(output), queue(capacity)
{
    closed = false;
    write_thread = std::thread([this] { write_loop(); });
}


void ConcurrentWriter::write_loop()
{
    record_t record;
    while(true)
    {
        record = queue.pop();
        if(record.buffer==nullptr)
            return;

        output->write(record.buffer, static_cast<std::streamsize>(record.length));
        delete[] record.buffer;
    }
}

void ConcurrentWriter::close()
{
    if(!closed)
    {
        queue.push( {nullptr, 0} );
        write_thread.join();
        closed=true;
    }
}
