//
// Created by steven on 7/18/17.
//
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include "doctest.h"
#include "../src/common/CompressedRecordFileWriter.h"
#include "../src/common/CompressedRecordFileReader.h"
#include "../src/common/Random.h"

TEST_CASE("CompressedFile")
{
    const unsigned int nrecords = 1000;
    const unsigned int data_size = 10 * 1024 * 1024; //10MB
    char *data = new char[data_size]; //uninitialized data

    Random r;
    for (unsigned int i = 0; i < data_size; i++)
        data[i] = (rand() % 20) ? 0 : static_cast<char>(r.random_uint<short>(0, 255));

    CompressedRecordFileWriter writer("compressedfile.test", nrecords);
    std::cout << writer.create_dictionary(data, data_size) << std::endl;

    std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();
    for (unsigned int i = 0; i < nrecords; i++)
    {
        writer.write_record(data + i * (data_size / nrecords), data_size / nrecords, false);
        CHECK(writer.get_uncompressed_size() == (i+1)*(data_size / nrecords));
    }
    writer.close();
    std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

    std::cout << "Compress ratio: " << writer.get_compressed_size() << " / " << writer.get_uncompressed_size() << " = "
              << static_cast<double>(writer.get_compressed_size())/writer.get_uncompressed_size() << "\n"
              << "Compress speed: " << data_size/(1024*1024*delta_t.count()) << "MiB/s" << std::endl;

    CompressedRecordFileReader reader("compressedfile.test");
    tstart = std::chrono::steady_clock::now();
    unsigned int i=nrecords;
    while(i)
    {
        CompressedRecord result = reader.get_record(--i);
        result.free();
    }
    delta_t = std::chrono::steady_clock::now() - tstart;
    std::cout <<  "Decompress speed: " << data_size/(1024*1024*delta_t.count()) << "MiB/s" << std::endl;


    i=nrecords;
    while(i)
    {
        CompressedRecord result = reader.get_record(--i);
        CHECK( result.length() == data_size/nrecords );
        CHECK( memcmp( result.get(), data+i*(data_size/nrecords), data_size/nrecords ) == 0 );
        result.free();
    }
    reader.close();

    delete[] data;
}


