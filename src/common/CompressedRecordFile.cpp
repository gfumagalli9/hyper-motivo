//
// Created by steven on 7/18/17.
//

#include <stdexcept>
#include <cassert>
#include "CompressedRecordFile.h"
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
    memcpy(&num_of_records, fdmap, sizeof(uint64_t));
    memcpy(&dictionary_size, fdmap + sizeof(uint64_t), sizeof(uint64_t));
    assert(dictionary_size <= 65536);

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
    const uint64_t record_length = offsets[record_no + 1].file_offset - offsets[record_no].file_offset;

    if (record_length == 0)
        return CompressedRecord(nullptr, 0, false);

    if (!offsets[record_no].compressed)
        return CompressedRecord(fdmap + offsets[record_no].file_offset, record_length, false);

    unsigned int mul = 1u << offsets[record_no].exp;
    uint64_t uncompressed_size_ub = static_cast<uint64_t>(offsets[record_no].mantissa) * mul + (mul - 1);

    char *buffer = new char[uncompressed_size_ub];
    LZ4_streamDecode_t decoder;
    LZ4_setStreamDecode(&decoder, dictionary, static_cast<int>(dictionary_size));

    uint64_t decompressed_bytes = 0;
    if (offsets[record_no].multi_block)
    {
        uint64_t position = offsets[record_no].file_offset;
        while(position-offsets[record_no].file_offset<record_length)
        {
            uint32_t next_compressed_block_length;
            memcpy(&next_compressed_block_length, fdmap + position, sizeof(uint32_t));
            position += sizeof(uint32_t);

            const int next_uncompressed_block_length_ub = (uncompressed_size_ub-decompressed_bytes<=MAX_BLOCK_SIZE)?static_cast<int>(uncompressed_size_ub-decompressed_bytes):static_cast<int>(MAX_BLOCK_SIZE);
            //std::cout << "Will read block of compressed size: " << next_compressed_block_length << " Uncompressed UB: " << next_uncompressed_block_length_ub << std::endl;

            int r = LZ4_decompress_safe_continue(&decoder, fdmap + position, buffer+decompressed_bytes, static_cast<int>(next_compressed_block_length), next_uncompressed_block_length_ub);
            assert(r>0);
            decompressed_bytes += static_cast<unsigned int>(r);
            position += next_compressed_block_length;
        }
    }
    else
    {
        assert(record_length<=MAX_BLOCK_SIZE);
        int r = LZ4_decompress_safe_continue(&decoder, fdmap + offsets[record_no].file_offset, buffer, static_cast<int>(record_length), static_cast<int>(uncompressed_size_ub));
        assert(r>0);
        decompressed_bytes += static_cast<unsigned int>(r);
    }

    return CompressedRecord(buffer, decompressed_bytes, true);
}


void CompressedRecordFileReader::close()
{
    delete[] dictionary;
    motivo_munmap(fdmap, file_length);
    fclose(fd);
    fd = nullptr;
}








CompressedRecordFileWriter::CompressedRecordFileWriter(const std::string &filename, const uint64_t num_records) : number_of_records(num_records)
{
    fd = fopen(filename.c_str(), "wb");

    fwrite(&num_records, sizeof(uint64_t), 1, fd);
    bytes_compressed += sizeof(uint64_t);
    offsets = new record_offset_t[number_of_records+1];

    LZ4_resetStream(&encoder);
    dictionary = nullptr;
    written_records=0;
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

    assert(position <= 0xFFFFFFFF);
    offsets[number_of_records].file_offset=position & 0xFFFFFFFF;
    offsets[number_of_records].mantissa=0;
    offsets[number_of_records].exp=0;

    fseeko(fd, static_cast<off_t>(2*sizeof(uint64_t) + dictionary_size), SEEK_SET);
    fwrite(offsets, sizeof(record_offset_t), number_of_records+1, fd);
    bytes_compressed +=  sizeof(record_offset_t) * (number_of_records+1);

    fclose(fd);
    fd=nullptr;
    delete[] offsets;
}

void CompressedRecordFileWriter::write_record(char *record, uint64_t length, double compress_threshold)
{
    assert(position <= 0xFFFFFFFF);
    offsets[written_records].file_offset=position & 0xFFFFFFFF;

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

    bool write_compressed = false;
    char *buffer = nullptr;
    uint64_t compressed_size = 0;
    if(compress_threshold > 0)
    {
        LZ4_loadDict(&encoder, dictionary, static_cast<int>(dictionary_size));
        uint64_t maxsize_blocks = length/MAX_BLOCK_SIZE;
        unsigned int remainder = static_cast<unsigned int>(length%MAX_BLOCK_SIZE);


        if(maxsize_blocks==0 || (maxsize_blocks==1 && remainder==0))
        {
            offsets[written_records].multi_block = false;
            int size_ub = static_cast<int>(LZ4_COMPRESSBOUND(length));
            buffer = new char[size_ub];
            int r = LZ4_compress_fast_continue(&encoder, record, buffer, static_cast<int>(length), size_ub, 1);
            assert(r>0);
            compressed_size = static_cast<uint64_t>(r);
        }
        else
        {
            offsets[written_records].multi_block = true;
            uint64_t size_ub = maxsize_blocks*LZ4_COMPRESSBOUND(MAX_BLOCK_SIZE) + LZ4_COMPRESSBOUND(remainder) + maxsize_blocks*sizeof(uint32_t);
            buffer = new char[size_ub];

            uint64_t processed=0;
            while(processed<length)
            {
                int block_size = (length-processed<=MAX_BLOCK_SIZE)?static_cast<int>(length-processed):static_cast<int>(MAX_BLOCK_SIZE);
                int r = LZ4_compress_fast_continue(&encoder, record+processed, buffer+compressed_size+sizeof(uint32_t), block_size, LZ4_COMPRESSBOUND(block_size), 1);
                assert(r>0);

                uint32_t compressed_block_size = static_cast<uint32_t>(r);
                memcpy(buffer+compressed_size, &compressed_block_size, sizeof(uint32_t));
                compressed_size += sizeof(uint32_t) + compressed_block_size;

                processed+= static_cast<uint64_t>(block_size);
                //std::cout << "Written block. Uncompressed size: " << block_size << " Compressed size: " << compressed_block_size << std::endl;
            }
        }

        write_compressed = (static_cast<double>(compressed_size) < static_cast<double>(length) * compress_threshold);
    }

    if(write_compressed)
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
        if(length>=MAX_BLOCK_SIZE)
            length=MAX_BLOCK_SIZE;

        LZ4_resetStream(&encoder);
        int size_ub = static_cast<int>(LZ4_COMPRESSBOUND(length));
        char *buffer = new char[size_ub];

        LZ4_compress_fast_continue(&encoder, data, buffer, static_cast<int>(length), size_ub, 1);
        dictionary_size = static_cast<uint64_t>(LZ4_saveDict(&encoder, dictionary, WANTED_DICTIONARY_SIZE));

        delete[] buffer;
        LZ4_resetStream(&encoder);
    }

    fwrite(&dictionary_size, sizeof(uint64_t), 1, fd);
    fwrite(dictionary, dictionary_size, 1, fd);
    bytes_compressed += sizeof(uint64_t) + dictionary_size;


    position = 2*sizeof(uint64_t) + dictionary_size + (number_of_records+1)*sizeof(record_offset_t); ;
    fseeko(fd, static_cast<off_t>(position), SEEK_SET);

    return dictionary_size;
}

