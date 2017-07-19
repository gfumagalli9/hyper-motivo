//
// Created by steven on 7/18/17.
//

#ifndef MOTIVO_COMPRESSEDRECORDFILEREADER_H
#define MOTIVO_COMPRESSEDRECORDFILEREADER_H

#include <utility>
#include <cstdint>
#include <string>
#include <lz4.h>

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

class CompressedRecordFileReader
{

private:
    struct record_offset_t //FIXME: Avoid duplication
    {
        uint64_t file_offset : 40;
        bool compressed : 1;
        uint8_t exp : 7;
        uint16_t mantissa; //mantissa * 2^exp is an upper-bound to the uncompressed size
    };

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


#endif //MOTIVO_COMPRESSEDRECORDFILE_H
