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

#include <cassert>
#include <cstring>
#include "RecordCompressor.h"

char* RecordCompressor::compress(const char *record, const uint64_t length, uint64_t *compressed_size)
{
    if(length==0)
        return nullptr;

    header_t header; // NOLINT
    header.compressed=true;
    //FIXME: We can do this faster
    header.exp = 0;
    uint64_t mantissa = length;
    while( mantissa & ~header_t::mantissa_mask )
    {
        header.exp++;
        mantissa >>= 1;
    }

    assert(mantissa <= std::numeric_limits<uint8_t>::max() );
    header.mantissa=static_cast<uint8_t>(mantissa);

    char* buffer = nullptr;
    *compressed_size = sizeof(header_t);

    LZ4_stream_t encoder;
    LZ4_resetStream(&encoder);
    LZ4_loadDict(&encoder, nullptr, 0);

    uint64_t maxsize_blocks = length/MAX_BLOCK_SIZE;
    auto remainder = static_cast<unsigned int>(length%MAX_BLOCK_SIZE);


    if(maxsize_blocks==0 || (maxsize_blocks==1 && remainder==0))
    {
        header.multi_block = false;
        auto size_ub = static_cast<int>(LZ4_COMPRESSBOUND(length));
        buffer = new char[sizeof(header_t) + static_cast<unsigned int>(size_ub)];
        int r = LZ4_compress_fast_continue(&encoder, record, buffer+sizeof(header_t), static_cast<int>(length), size_ub, 1);
        assert(r>0);
        *compressed_size += static_cast<uint64_t>(r);
    }
    else
    {
        header.multi_block = true;
        uint64_t size_ub = maxsize_blocks*LZ4_COMPRESSBOUND(MAX_BLOCK_SIZE) + LZ4_COMPRESSBOUND(remainder) + maxsize_blocks*sizeof(uint32_t);
        buffer = new char[size_ub + sizeof(header_t)];

        uint64_t processed=0;
        while(processed<length)
        {
            int block_size = (length-processed<=MAX_BLOCK_SIZE)?static_cast<int>(length-processed):static_cast<int>(MAX_BLOCK_SIZE);
            int r = LZ4_compress_fast_continue(&encoder, record+processed, buffer+*compressed_size+sizeof(uint32_t), block_size, LZ4_COMPRESSBOUND(block_size), 1);
            assert(r>0);

            auto compressed_block_size = static_cast<uint32_t>(r);
            memcpy(buffer+*compressed_size, &compressed_block_size, sizeof(uint32_t));
            *compressed_size += sizeof(uint32_t) + compressed_block_size;

            processed+= static_cast<uint64_t>(block_size);
        }
    }

    memcpy(buffer, &header, sizeof(header_t));
    return buffer;
}


