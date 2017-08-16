//
// Created by steven on 8/5/17.
//

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
        bool allocated;
    };

    struct [[gnu::packed]] header_t
    {
        static constexpr const uint64_t mantissa_mask = 0x00000000000000FF;
        header_t() = default;


        uint8_t mantissa; //mantissa * 2^exp + (2^exp-1) is an upper-bound to the uncompressed size
        uint8_t exp : 6;
        bool compressed : 1;
        bool multi_block : 1;
    };

    static_assert( sizeof(header_t) == 2, "Structure record_offset_t is not packed." );
    static constexpr const header_t uncompressed_header = {0, 0, true, false};

    static char* compress(const char *record, const uint64_t length, uint64_t *compressed_size);

    template<typename T, bool RAW> static decompress_result_t<T> decompress(const char *record, const uint64_t length)
    {
        if(length<sizeof(header_t))
            return decompress_result_t<T>{nullptr, 0, false};

        header_t header;
        memcpy(&header, record, sizeof(header_t));

        if (!header.compressed)
        {
            assert((length- sizeof(header_t))%sizeof(T)==0);

            static_assert(!RAW || alignof(T)==1, "Raw read allowed but type is not 1-byte aligned");
            if(RAW)
                return decompress_result_t<T>{reinterpret_cast<T*>(record+sizeof(header_t)), (length - sizeof(header_t))/sizeof(T), false};

            typename std::remove_const<T>::type* buffer = new typename std::remove_const<T>::type[(length- sizeof(header_t))/sizeof(T)];
            memcpy(buffer, record+sizeof(header_t), (length- sizeof(header_t)));
            return decompress_result_t<T>{buffer, (length- sizeof(header_t))/sizeof(T), true};
        }

        unsigned int mul = 1u << header.exp;
        uint64_t uncompressed_size_ub = static_cast<uint64_t>(header.mantissa) * mul + (mul - 1);
        uncompressed_size_ub -= uncompressed_size_ub%sizeof(T);
        assert(uncompressed_size_ub>0);

        typename std::remove_const<T>::type* buffer = new typename std::remove_const<T>::type[uncompressed_size_ub/sizeof(T)];
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
                int r = LZ4_decompress_safe_continue(&decoder, record + position, reinterpret_cast<char*>(buffer)+decompressed_bytes, static_cast<int>(next_compressed_block_length), next_uncompressed_block_length_ub);
                assert(r>0);
                decompressed_bytes += static_cast<unsigned int>(r);
                position += next_compressed_block_length;
            }
        }
        else
        {
            assert(length-sizeof(header_t)<=MAX_BLOCK_SIZE);
            int r = LZ4_decompress_safe_continue(&decoder, record + sizeof(header_t), reinterpret_cast<char*>(buffer), static_cast<int>(length-sizeof(header_t)), static_cast<int>(uncompressed_size_ub));
            assert(r>0);
            decompressed_bytes = static_cast<unsigned int>(r);
        }

        assert(decompressed_bytes%sizeof(T)==0);

        return decompress_result_t<T>{buffer, decompressed_bytes/sizeof(T), true};
    }
};


#endif //MOTIVO_COMPRESSOR_H
