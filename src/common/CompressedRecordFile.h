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

static_assert(LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) <= std::numeric_limits<int>::max(), "LZ4_COMPRESSBOUND(LZ4_MAX_INPUT_SIZE) does not fit in a int");
static_assert(LZ4_MAX_INPUT_SIZE <= std::numeric_limits<int>::max(), "LZ4_MAX_INPUT_SIZE does not fit in a int");

constexpr uint32_t MAX_BLOCK_SIZE = (LZ4_MAX_INPUT_SIZE<=std::numeric_limits<uint32_t>::max())?LZ4_MAX_INPUT_SIZE:std::numeric_limits<uint32_t>::max();

class CompressedRecord
{
private:
    char* ptr;
    uint64_t len;
    bool needs_free;

public:
    explicit CompressedRecord(char* ptr, uint64_t len, bool needs_free) noexcept : ptr(ptr), len(len), needs_free(needs_free) {}

    explicit operator bool() const noexcept { return ptr == nullptr; }
    const void* get() const noexcept { return ptr; }
    uint64_t length() const noexcept { return len; }
    void free() { if(ptr && needs_free) delete[] ptr; ptr= nullptr; }
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
    record_offset_t* offsets;

    uint64_t dictionary_size;
    char* dictionary;

public:
    CompressedRecordFileReader(const std::string& filename);
    ~CompressedRecordFileReader();

    uint64_t number_of_records() const { return num_of_records; }
    CompressedRecord get_record(const uint64_t record_no);
    void close();


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
