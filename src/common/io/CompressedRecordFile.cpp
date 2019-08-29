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

#include <stdexcept>
#include <cassert>
#include "CompressedRecordFile.h"

CompressedRecordFileWriter::CompressedRecordFileWriter(const std::string &filename, const uint64_t num_records) : number_of_records(num_records)
{
    fd = fopen(filename.c_str(), "wb");

    fwrite(&num_records, sizeof(uint64_t), 1, fd);
    bytes_compressed = sizeof(uint64_t);
    bytes_uncompressed=0;
    offsets = new record_offset_t[number_of_records+1];
    written_records=0;

    position = sizeof(uint64_t) + (number_of_records+1)*sizeof(record_offset_t);
    fseeko(fd, static_cast<off_t>(position), SEEK_SET);
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

    if(written_records!=number_of_records)
        throw std::runtime_error("Not all records have been written");

    assert( (position & ~record_offset_t::file_offset_mask) == 0);
    offsets[number_of_records].file_offset=position & record_offset_t::file_offset_mask;

    fseeko(fd, static_cast<off_t>(sizeof(uint64_t)), SEEK_SET);
    fwrite(offsets, sizeof(record_offset_t), number_of_records+1, fd);
    bytes_compressed +=  sizeof(record_offset_t) * (number_of_records+1);

    fclose(fd);
    fd=nullptr;
    delete[] offsets;
}

void CompressedRecordFileWriter::write_record(char *record, uint64_t length, double compress_threshold)
{
    assert( (position & ~record_offset_t::file_offset_mask) == 0);
    offsets[written_records].file_offset=position & record_offset_t::file_offset_mask;

    if(length==0)
    {
        written_records++;
        return;
    }

    bool write_compressed = false;
    uint64_t compressed_size = 0;
    char *buffer = nullptr;

    if(compress_threshold>0)
    {
        buffer = RecordCompressor::compress(record, length, &compressed_size);
        write_compressed = static_cast<double>(compressed_size) < static_cast<double>(length) * compress_threshold;
    }

    if(write_compressed)
    {
        fwrite(buffer, compressed_size, 1, fd);
        position += compressed_size;
        bytes_compressed += compressed_size;
    }
    else
    {
        fwrite(&RecordCompressor::uncompressed_header, sizeof(RecordCompressor::uncompressed_header), 1, fd);
        fwrite(record, length, 1, fd);
        position += length + sizeof(RecordCompressor::header_t);
        bytes_compressed += length;
    }

    delete[] buffer;

    written_records++;
    bytes_uncompressed += length;
}

