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

#ifndef MOTIVO_COMPRESSEDRECORDFILEREADER_H
#define MOTIVO_COMPRESSEDRECORDFILEREADER_H

#include <utility>
#include <cstdio>
#include <cstdint>
#include <string>
#include <limits>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include "RecordCompressor.h"
#include "BaseRecordSource.h"
#include "../platform/platform.h"

template<typename T, bool RAW> class CompressedRecordFileReader : public BaseRecordSource<T>
{
private:
    FILE* fd = nullptr;
    char* fdmap = nullptr;
    size_t file_length = 0;
    uint64_t num_of_records = 0;
    char* offsets = nullptr;

public:
    CompressedRecordFileReader() = default;
    explicit CompressedRecordFileReader(const std::string& filename)
    {
        open(filename);
    }

    ~CompressedRecordFileReader()
    {
        if(fd != nullptr)
            close();
    }

    void open(const std::string &filename)
    {
        //Open file
        if(fd != nullptr)
            throw std::runtime_error("A file is already open");

        fd = fopen(filename.c_str(), "rb" );
        if(fd == NULL)
        {
            fd = nullptr;
            throw std::runtime_error("Could not open file " + filename);
        }

        //Map file
        fseek(fd, 0, SEEK_END);
        file_length = static_cast<size_t>(ftello(fd)); //FIXME: Check for errors
        fdmap = static_cast<char*>(motivo_mmap(file_length, PROT_READ, fileno(fd)));
        assert(fdmap!=MAP_FAILED);


        //Read number of records and set up offsets pointer
        memcpy(&num_of_records, this->fdmap, sizeof(uint64_t));
        offsets = fdmap + sizeof(uint64_t);
        motivo_prefault(0, sizeof(uint64_t)*(num_of_records + 1), fileno(fd));
    }

    uint64_t number_of_records() const { return num_of_records; }

    void prefault(const uint64_t from, const uint64_t to)
    {
        record_offset_t from_offset, to_offset; // NOLINT
        memcpy(&from_offset, offsets + from*sizeof(record_offset_t), sizeof(record_offset_t));
        memcpy(&to_offset, offsets + (to+1)*sizeof(record_offset_t), sizeof(record_offset_t));

        motivo_prefault(from_offset.file_offset, to_offset.file_offset-from_offset.file_offset, fileno(fd));
    }

    void close()
    {
        motivo_munmap(fdmap, file_length);
        fclose(fd);
        fd = nullptr;
    }


    Record<const char> get_raw(const uint64_t record_no)
    {
        record_offset_t offset, next_offset; // NOLINT
        memcpy(&offset, offsets + record_no*sizeof(record_offset_t), sizeof(record_offset_t));
        memcpy(&next_offset, offsets + (record_no+1)*sizeof(record_offset_t), sizeof(record_offset_t));

        return Record<const char>(fdmap+offset.file_offset, next_offset.file_offset - offset.file_offset, nullptr);
    }

    Record<T> get_record(const uint64_t record_no) const
    {
        record_offset_t offset, next_offset; // NOLINT
        memcpy(&offset, offsets + record_no*sizeof(record_offset_t), sizeof(record_offset_t));
        memcpy(&next_offset, offsets + (record_no+1)*sizeof(record_offset_t), sizeof(record_offset_t));

        RecordCompressor::decompress_result_t<T> result = RecordCompressor::decompress<T, RAW>(fdmap+offset.file_offset, next_offset.file_offset - offset.file_offset);
        return Record<T>(result.ptr, result.len, result.allocated_ptr);
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

public:
    CompressedRecordFileWriter(const std::string &filename, uint64_t num_records);
    ~CompressedRecordFileWriter();

    uint64_t get_compressed_size() const { return bytes_compressed; }
    uint64_t get_uncompressed_size() const { return bytes_uncompressed; }

    void write_record(char* record, uint64_t length, double compress_threshold=1);
    void close();
};


#endif //MOTIVO_COMPRESSEDRECORDFILE_H
