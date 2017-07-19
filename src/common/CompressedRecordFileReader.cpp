//
// Created by steven on 7/18/17.
//

#include <stdexcept>
#include <cassert>
#include "CompressedRecordFileReader.h"
#include "../platform/platform.h"

CompressedRecordFileReader::CompressedRecordFileReader(const std::string &filename)
{
    //Open file
    fd = fopen( filename.c_str(), "rb" );
    if(fd==NULL)
        throw std::runtime_error("Could not open file " + filename);

    //Map file
    fseek(fd, 0, SEEK_END);
    file_length = static_cast<size_t>(ftello(fd)); //FIXME: Check for errors
    fdmap = static_cast<char*>(motivo_mmap(file_length, PROT_READ, fileno(fd)));
    assert(fdmap!=MAP_FAILED);


    //Read number of records, dictionary, and set up offsets pointer
    num_of_records = *(reinterpret_cast<uint64_t*>(fdmap));
    dictionary_size = *(reinterpret_cast<uint64_t*>(fdmap + sizeof(uint64_t)));
    dictionary = new char[dictionary_size];
    memcpy(dictionary, fdmap + 2*sizeof(uint64_t), dictionary_size);
    offsets = reinterpret_cast<record_offset_t*>(fdmap +  2*sizeof(uint64_t) + dictionary_size);
}

CompressedRecordFileReader::~CompressedRecordFileReader()
{
    if(fd != nullptr)
        close();
}

CompressedRecord CompressedRecordFileReader::get_record(const uint64_t record_no)
{
    uint64_t record_length = offsets[record_no+1].file_offset - offsets[record_no].file_offset;

    if(record_length==0)
        return CompressedRecord(nullptr, 0, false);

    if(!offsets[record_no].compressed)
        return CompressedRecord(fdmap + offsets[record_no].file_offset, record_length, false);

    uint64_t mul = static_cast<uint64_t>(1) << offsets[record_no].exp;
    uint64_t uncompressed_size_ub = offsets[record_no].mantissa * mul + (mul-1);

    char* buffer = new char[uncompressed_size_ub];
    LZ4_streamDecode_t decoder;
    LZ4_setStreamDecode(&decoder, dictionary, dictionary_size);
    const int decompressed_bytes = LZ4_decompress_safe_continue(&decoder, fdmap + offsets[record_no].file_offset, buffer, record_length, uncompressed_size_ub);

    return CompressedRecord(buffer, decompressed_bytes, true);
}

void CompressedRecordFileReader::close()
{
    delete[] dictionary;
    motivo_munmap(fdmap, file_length);
    fclose(fd);
    fd = nullptr;
}
