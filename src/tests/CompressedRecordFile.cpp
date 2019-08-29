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

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include "doctest.h"
#include "../common/io/CompressedRecordFile.h"
#include "../common/random/Random.h"

void test(const uint64_t data_size, const unsigned int nrecords, const bool random)
{
    auto data = new char[data_size]; //uninitialized data

    Random r;
    for (uint64_t i = 0; i < data_size; i++)
        data[i] = random?static_cast<char>(r.random_uint<short>(0, 255)):static_cast<char>(i%256);

    CompressedRecordFileWriter writer("compressedfile.test", nrecords);

    std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < nrecords; i++)
        writer.write_record(data + i * (data_size / nrecords), data_size / nrecords, 1.2);
    writer.close();
    std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

    std::cout << "Compress ratio: " << writer.get_compressed_size() << " / " << writer.get_uncompressed_size() << " = "
              << static_cast<double>(writer.get_compressed_size())/static_cast<double>(writer.get_uncompressed_size()) << "\n"
              << "Compress speed: " << static_cast<double>(data_size)/(1024*1024*delta_t.count()) << "MiB/s" << std::endl;

    CompressedRecordFileReader<const char, true> reader("compressedfile.test");
    tstart = std::chrono::steady_clock::now();
    for(uint64_t i=1; i<=nrecords; i++)
    {
        Record<const char> result = reader.get_record(nrecords-i);
        result.free();
    }
    delta_t = std::chrono::steady_clock::now() - tstart;
    std::cout <<  "Decompress speed: " << static_cast<double>(data_size)/(1024*1024*delta_t.count()) << "MiB/s" << std::endl;


    for(uint64_t i=0; i<nrecords; i++)
    {
        Record<const char> result = reader.get_record(i);
        CHECK( result.length() == data_size/nrecords );
        CHECK( memcmp( result.begin(), data+i*(data_size/nrecords), data_size/nrecords ) == 0 );
        result.free();
    }
    reader.close();

    delete[] data;
    std::remove("compressedfile.test");
}

TEST_CASE("CompressedRecordFile One")
{
    test(1024L * 1024, 1, false); //1MB, 1 record
}


TEST_CASE("CompressedRecordFile")
{
    test(10 * 1024L * 1024, 1000, true); //10MB, 1000 records
}

TEST_CASE("CompressedRecordFile Multiblock slow")
{
    test(4 * 1024L * 1024 * 1024 + 1, 1, false); //4GB+1byte, 1 record
}

