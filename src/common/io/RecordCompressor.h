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

#ifndef MOTIVO_COMPRESSOR_H
#define MOTIVO_COMPRESSOR_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <cassert>
#include "lz4.h"

class RecordCompressor
{
private:
    static_assert(LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) <= std::numeric_limits<int>::max(), "LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) does not fit in a int");
    static_assert(LZ4_MAX_INPUT_SIZE <= std::numeric_limits<int>::max(), "LZ4_MAX_INPUT_SIZE does not fit in a int");

    static constexpr uint32_t MAX_BLOCK_SIZE = (LZ4_MAX_INPUT_SIZE<=std::numeric_limits<uint32_t>::max())?LZ4_MAX_INPUT_SIZE:std::numeric_limits<uint32_t>::max();


public:
    template <typename T> struct decompress_result_t
    {
        T* ptr;
        const uint64_t len;
        char* allocated_ptr;
    };

    struct [[gnu::packed]] header_t
    {
        static constexpr uint64_t mantissa_mask = 0x00000000000000FF;

        uint8_t mantissa; //mantissa * 2^exp + (2^exp-1) is an upper-bound to the uncompressed size
        uint8_t exp : 6;
        bool compressed : 1;
        bool multi_block : 1;
    };

    static_assert( sizeof(header_t) == 2, "Structure record_offset_t is not packed." );
    static constexpr header_t uncompressed_header {0, 0, false, false};

    static char* compress(const char *record, uint64_t length, uint64_t *compressed_size);

    template<typename T, bool RAW> static decompress_result_t<T> decompress(const char *record, const uint64_t length)
    {
        static_assert(std::is_standard_layout<T>::value, "template type T is not a standard layout type");
        static_assert(std::is_trivial<T>::value, "template type T is not a trivial type");

        if(length<sizeof(header_t))
            return decompress_result_t<T>{nullptr, 0, nullptr};

        header_t header; // NOLINT
        memcpy(&header, record, sizeof(header_t));

        if (!header.compressed)
        {
            assert((length- sizeof(header_t))%sizeof(T)==0);

            static_assert(!RAW || alignof(T)==1, "Raw read allowed but type is not 1-byte aligned");
            if(RAW)
                return decompress_result_t<T>{reinterpret_cast<T*>(record+sizeof(header_t)), (length - sizeof(header_t))/sizeof(T), nullptr};

            /* 5.3.4/10 - New
             * A new-expression passes the amount of space requested to the allocation function as the first argument of type std::
             * size_t. That argument shall be no less than the size of the object being created; it may be greater than the size of the
             * object being created only if the object is an array. For arrays of char and unsigned char, the difference between the
             * result of the new-expression and the address returned by the allocation function shall be an integral multiple of the most
             * stringent alignment requirement (3.9) of any object type whose size is no greater than the size of the array being created.
             * [Note: Because allocation functions are assumed to return pointers to storage that is appropriately aligned for objects
             * of any type, this constraint on array allocation overhead permits the common idiom of allocating character arrays into
             * which objects of other types will later be placed. — end note ]

             * 3.7.3.1/2 - Allocation functions
             * The pointer returned shall be suitably aligned so that it can be converted to a pointer of any complete object type and then
             * used to access the object or array in the storage allocated (until the storage is explicitly deallocated by a call to a
             * corresponding deallocatio function)
             *
             * TLDR: The next pointer is properly aligned. */
            auto buffer = new char[length- sizeof(header_t)];
            auto buffer_T = new (buffer) typename std::remove_const<T>::type[(length- sizeof(header_t))/sizeof(T)];
            memcpy(buffer, record+sizeof(header_t), (length- sizeof(header_t)));
            return decompress_result_t<T>{buffer_T, (length- sizeof(header_t))/sizeof(T), buffer};
        }

        unsigned int mul = 1u << header.exp;
        uint64_t uncompressed_size_ub = static_cast<uint64_t>(header.mantissa) * mul + (mul - 1);
        uncompressed_size_ub -= uncompressed_size_ub%sizeof(T);
        assert(uncompressed_size_ub>0);
        assert(uncompressed_size_ub%sizeof(T)==0);

        //Properly aligned
        auto buffer = new char[uncompressed_size_ub];
        auto buffer_T = new (buffer) typename std::remove_const<T>::type[uncompressed_size_ub/sizeof(T)];
        assert(buffer==reinterpret_cast<char*>(buffer_T));

        LZ4_streamDecode_t decoder;
        LZ4_setStreamDecode(&decoder, nullptr, 0);

        uint64_t decompressed_bytes = 0;
        if (header.multi_block)
        {
            uint64_t position = sizeof(header_t);
            while(position<length)
            {
                uint32_t next_compressed_block_length;
                memcpy(&next_compressed_block_length, record + position, sizeof(uint32_t));
                position += sizeof(uint32_t);

                const int next_uncompressed_block_length_ub = (uncompressed_size_ub-decompressed_bytes<=MAX_BLOCK_SIZE)?static_cast<int>(uncompressed_size_ub-decompressed_bytes):static_cast<int>(MAX_BLOCK_SIZE);
                int r = LZ4_decompress_safe_continue(&decoder, record + position, buffer+decompressed_bytes, static_cast<int>(next_compressed_block_length), next_uncompressed_block_length_ub);
                assert(r>0);
                decompressed_bytes += static_cast<unsigned int>(r);
                position += next_compressed_block_length;
            }
        }
        else
        {
            assert(length-sizeof(header_t)<=MAX_BLOCK_SIZE);
            int r = LZ4_decompress_safe_continue(&decoder, record + sizeof(header_t), buffer, static_cast<int>(length-sizeof(header_t)), static_cast<int>(uncompressed_size_ub));
            assert(r>0);
            decompressed_bytes = static_cast<unsigned int>(r);
        }

        assert(decompressed_bytes%sizeof(T)==0);
        return decompress_result_t<T>{buffer_T, decompressed_bytes/sizeof(T), buffer};
    }
};


#endif //MOTIVO_COMPRESSOR_H
