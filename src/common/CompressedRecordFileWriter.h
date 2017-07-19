//
// Created by steven on 7/18/17.
//

#ifndef MOTIVO_COMPRESSEDRECORDFILEWRITER_H
#define MOTIVO_COMPRESSEDRECORDFILEWRITER_H

#include <cstdint>
#include <string>
#include <lz4.h>

class CompressedRecordFileWriter
{
private:
    struct record_offset_t
    {
        uint64_t file_offset : 40;
        bool compressed : 1;
        uint8_t exp : 7;
        uint16_t mantissa; //mantissa * 2^exp is an upper-bound to the uncompressed size
    };

    static_assert( sizeof(record_offset_t) == 8, "Structure record_offset_t is not packed." );

    FILE* fd;
    const uint64_t number_of_records;
    uint64_t written_records;
    record_offset_t* offsets;
    uint64_t position;

    uint64_t bytes_compressed;
    uint64_t bytes_uncompressed;

    LZ4_stream_t encoder;
    static constexpr uint64_t WANTED_DICTIONARY_SIZE = 64 * 1024; //64K
    uint64_t dictionary_size;
    char* dictionary;

public:
    CompressedRecordFileWriter(const std::string &filename, const uint64_t num_records);
    ~CompressedRecordFileWriter();

    uint64_t get_compressed_size() const { return bytes_compressed; }
    uint64_t get_uncompressed_size() const { return bytes_uncompressed; }

    uint64_t create_dictionary(char* data, uint64_t length);
    void write_record(char* record, uint64_t length, bool allow_compression=true);
    void close();
};


#endif //MOTIVO_COMPRESSEDRECORDFILEWRITER_H
