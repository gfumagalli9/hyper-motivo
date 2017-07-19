//
// Created by steven on 7/18/17.
//

#include <cstring>
#include <stdexcept>
#include <limits>
#include <cassert>
#include "CompressedRecordFileWriter.h"

CompressedRecordFileWriter::CompressedRecordFileWriter(const std::string &filename, const uint64_t num_records) : number_of_records(num_records)
{
    fd = fopen(filename.c_str(), "wb");

    fwrite(&num_records, sizeof(uint64_t), 1, fd);
    bytes_compressed += sizeof(uint64_t);
    offsets = new record_offset_t[number_of_records+1];

    LZ4_resetStream(&encoder);
    dictionary = nullptr;
}

CompressedRecordFileWriter::~CompressedRecordFileWriter()
{
    if(fd!=nullptr)
        close();
}


void CompressedRecordFileWriter::close()
{
    if(fd==nullptr)
        return;

    if(dictionary!= nullptr)
        delete[] dictionary;

    if(written_records!=number_of_records)
        throw std::runtime_error("Not all records have been written");

    offsets[number_of_records].file_offset=position;
    offsets[number_of_records].mantissa=0;
    offsets[number_of_records].exp=0;

    fseeko(fd, 2*sizeof(uint64_t) + dictionary_size, SEEK_SET);
    fwrite(offsets, sizeof(record_offset_t), number_of_records+1, fd);
    bytes_compressed +=  sizeof(record_offset_t) * (number_of_records+1);

    fclose(fd);
    fd=nullptr;
    delete[] offsets;
}

void CompressedRecordFileWriter::write_record(char *record, uint64_t length, bool allow_compression)
{
    offsets[written_records].file_offset=position;

    //FIXME: We can do this faster
    static constexpr uint64_t mask = ~static_cast<uint64_t>(std::numeric_limits<uint16_t>::max());
    offsets[written_records].exp = 0;
    uint64_t mantissa = length;
    while( mantissa & mask )
    {
        offsets[written_records].exp++;
        mantissa >>= 1;
    }

    assert(mantissa <= std::numeric_limits<uint16_t>::max() );
    offsets[written_records].mantissa=static_cast<uint16_t>(mantissa);

    if(length==0)
    {
        written_records++;
        return;
    }

    char *buffer = nullptr;
    int compressed_size;
    if(allow_compression)
    {
        uint64_t size_ub = LZ4_COMPRESSBOUND(length);
        buffer = new char[size_ub];

        //LZ4_loadDict(&encoder, dictionary, dictionary_size);
        LZ4_loadDict(&encoder, nullptr, 0);
        compressed_size = LZ4_compress_fast_continue(&encoder, record, buffer, length, size_ub, 1);
        assert(compressed_size>0);
    }

    if(allow_compression && compressed_size < length)
    {
        fwrite(buffer, compressed_size, 1, fd);
        offsets[written_records].compressed = true;
        position += compressed_size;
        bytes_compressed += compressed_size;
    }
    else
    {
        fwrite(record, length, 1, fd);
        offsets[written_records].compressed = false;
        position += length;
        bytes_compressed += length;
    }

    if(buffer!=nullptr)
        delete[] buffer;

    written_records++;
    bytes_uncompressed += length;
}

uint64_t CompressedRecordFileWriter::create_dictionary(char *data, uint64_t length)
{
    if(dictionary != nullptr)
        throw std::runtime_error("Dictionary can only be created once");

    dictionary = new char[WANTED_DICTIONARY_SIZE];
    memset(dictionary, 0, WANTED_DICTIONARY_SIZE);
    dictionary_size=0;

    if(length!=0)
    {
        LZ4_resetStream(&encoder);
        uint64_t size_ub = LZ4_COMPRESSBOUND(length);
        char *buffer = new char[size_ub];

        LZ4_compress_fast_continue(&encoder, data, buffer, length, size_ub, 1);
        dictionary_size = LZ4_saveDict(&encoder, dictionary, WANTED_DICTIONARY_SIZE);

        delete[] buffer;
        LZ4_resetStream(&encoder);
    }

    fwrite(&dictionary_size, sizeof(uint64_t), 1, fd);
    fwrite(dictionary, dictionary_size, 1, fd);
    bytes_compressed += sizeof(uint64_t) + dictionary_size;


    position = 2*sizeof(uint64_t) + dictionary_size + (number_of_records+1)*sizeof(record_offset_t); ;
    fseeko(fd, position, SEEK_SET);

    return dictionary_size;
}

