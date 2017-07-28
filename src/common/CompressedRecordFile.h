//
// Created by steven on 7/18/17.
//

#ifndef MOTIVO_COMPRESSEDRECORDFILEREADER_H
#define MOTIVO_COMPRESSEDRECORDFILEREADER_H

#include <utility>
#include <cstdint>
#include <string>
#include <lz4.h>
#include <limits>
#include <cstring>
#include <cassert>

static_assert(LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) <= std::numeric_limits<int>::max(), "LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) does not fit in a int");
static_assert(LZ4_MAX_INPUT_SIZE <= std::numeric_limits<int>::max(), "LZ4_MAX_INPUT_SIZE does not fit in a int");

constexpr uint32_t MAX_BLOCK_SIZE = (LZ4_MAX_INPUT_SIZE<=std::numeric_limits<uint32_t>::max())?LZ4_MAX_INPUT_SIZE:std::numeric_limits<uint32_t>::max();

template<typename T> class CompressedRecord
{
private:
    T* ptr;
    const uint64_t len;
    const bool needs_free;

public:
    CompressedRecord(T* ptr, uint64_t len, bool needs_free=true) noexcept : ptr(ptr), len(len), needs_free(needs_free) {}

    uint64_t length() const noexcept { return len; }
    const T* begin() const noexcept { return ptr; }
    const T* end() const noexcept { return ptr+len; }
    void free() { if(needs_free && ptr) delete[] ptr; ptr=nullptr; }
};

struct [[gnu::packed]] record_offset_t
{
    uint64_t file_offset : 40; //Max 1 TB
    uint16_t mantissa; //mantissa * 2^exp + (2^exp-1) is an upper-bound to the uncompressed size
    uint8_t exp : 6;
    bool compressed : 1;
    bool multi_block : 1;
};

class CompressedRecordFileReader
{
private:
    static_assert( sizeof(record_offset_t) == 8, "Structure record_offset_t is not packed." );

    FILE* fd;
    char* fdmap;
    size_t file_length;
    uint64_t num_of_records;
    char* offsets;

    uint64_t dictionary_size;
    char* dictionary;

public:
    CompressedRecordFileReader(const std::string& filename);
    ~CompressedRecordFileReader();

    uint64_t number_of_records() const { return num_of_records; }
    void close();


    template<typename T, bool RAW> CompressedRecord<T> get_record(const uint64_t record_no)
    {
        record_offset_t offset,next_offset;
        memcpy(&offset, offsets + record_no*sizeof(record_offset_t), sizeof(record_offset_t));
        memcpy(&next_offset, offsets + (record_no+1)*sizeof(record_offset_t), sizeof(record_offset_t));
        const uint64_t record_length = next_offset.file_offset - offset.file_offset;

        if (record_length == 0)
            return CompressedRecord<T>(nullptr, 0);

        if (!offset.compressed)
        {
            assert(record_length%sizeof(T)==0);

            static_assert(!RAW || alignof(T)==1, "Raw read allowed but type is not 1-byte aligned");
            if(RAW)
                return CompressedRecord<T>(reinterpret_cast<T*>(fdmap+offset.file_offset), record_length/sizeof(T), false);

            T* buffer = new T[record_length/sizeof(T)];
            memcpy(buffer, fdmap+offset.file_offset, record_length);
            return CompressedRecord<T>(buffer, record_length/sizeof(T));
        }

        unsigned int mul = 1u << offset.exp;
        uint64_t uncompressed_size_ub = static_cast<uint64_t>(offset.mantissa) * mul + (mul - 1);
        uncompressed_size_ub -= uncompressed_size_ub%sizeof(T);
        assert(uncompressed_size_ub>0);

        T* buffer = new T[uncompressed_size_ub/sizeof(T)];
        LZ4_streamDecode_t decoder;
        LZ4_setStreamDecode(&decoder, dictionary, static_cast<int>(dictionary_size));

        uint64_t decompressed_bytes = 0;
        if (offset.multi_block)
        {
            uint64_t position = offset.file_offset;
            while(position-offset.file_offset<record_length)
            {
                uint32_t next_compressed_block_length;
                memcpy(&next_compressed_block_length, fdmap + position, sizeof(uint32_t));
                position += sizeof(uint32_t);

                const int next_uncompressed_block_length_ub = (uncompressed_size_ub-decompressed_bytes<=MAX_BLOCK_SIZE)?static_cast<int>(uncompressed_size_ub-decompressed_bytes):static_cast<int>(MAX_BLOCK_SIZE);
                int r = LZ4_decompress_safe_continue(&decoder, fdmap + position, reinterpret_cast<char*>(buffer)+decompressed_bytes, static_cast<int>(next_compressed_block_length), next_uncompressed_block_length_ub);
                assert(r>0);
                decompressed_bytes += static_cast<unsigned int>(r);
                position += next_compressed_block_length;
            }
        }
        else
        {
            assert(record_length<=MAX_BLOCK_SIZE);
            int r = LZ4_decompress_safe_continue(&decoder, fdmap + offset.file_offset, reinterpret_cast<char*>(buffer), static_cast<int>(record_length), static_cast<int>(uncompressed_size_ub));
            assert(r>0);
            decompressed_bytes = static_cast<unsigned int>(r);
        }

        assert(decompressed_bytes%sizeof(T)==0);
        return CompressedRecord<T>(buffer, decompressed_bytes/sizeof(T));
    }

};


class CompressedRecordFileWriter
{
private:
    FILE* fd;
    const uint64_t number_of_records;
    uint64_t written_records;
    record_offset_t* offsets;
    uint64_t position;

    uint64_t bytes_compressed;
    uint64_t bytes_uncompressed;

    LZ4_stream_t encoder;

    static constexpr int WANTED_DICTIONARY_SIZE = (65536<=std::numeric_limits<int>::max())?65536:std::numeric_limits<int>::max(); //64K
    uint64_t dictionary_size;
    char* dictionary;

public:
    CompressedRecordFileWriter(const std::string &filename, const uint64_t num_records);
    ~CompressedRecordFileWriter();

    uint64_t get_compressed_size() const { return bytes_compressed; }
    uint64_t get_uncompressed_size() const { return bytes_uncompressed; }

    uint64_t create_dictionary(char* data, uint64_t length);
    void write_record(char* record, uint64_t length, double compress_threshold=1);
    void close();
};


#endif //MOTIVO_COMPRESSEDRECORDFILE_H
