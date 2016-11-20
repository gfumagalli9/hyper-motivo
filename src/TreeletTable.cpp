//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include <stdexcept>
#include <sys/mman.h>

TreeletTable::TreeletTable(const std::string& basename)
{
    offsets_fd = fopen( (basename+".off").c_str(), "rb" );
    fread(&size, sizeof(uint64_t), 1, offsets_fd);
    offsets = static_cast<uint64_t*>(mmap(NULL, (size+1)*sizeof(uint64_t), PROT_READ, MAP_PRIVATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 1;

    data_fd = fopen( (basename+".dat").c_str(), "rb" );
    data = static_cast<treelet_count_pair*>(mmap(NULL, offsets[size] * sizeof(treelet_count_pair), PROT_READ, MAP_PRIVATE, fileno(data_fd), 0));
    assert(data!=MAP_FAILED);

    FILE* hashes_fd = fopen( (basename+".phf").c_str(), "rb" );
    if(hashes_fd!=NULL)
    {
        long header_size = (size+7)/8; //I.e, ceil(size/8)
        uint8_t* hashes_bitmask = new uint8_t[header_size];
        fread(hashes_bitmask, 1, header_size, hashes_fd);

        hashes = new cmph_t*[size];
        for(int u=0; u<size; u++)
        {
            if( hashes_bitmask[u/8] & (0b10000000u >> u%8) )
                hashes[u] = cmph_load(hashes_fd);
            else
                hashes[u] = NULL;
        }
        fclose(hashes_fd);
    }
    else
        hashes = NULL;
}

TreeletTable::~TreeletTable()
{
    if(hashes != NULL)
    {
        for(int u = 0; u < size; u++)
            if(hashes[u]!=NULL)
                cmph_destroy(hashes[u]);

        delete[] hashes;
    }

    munmap(data, size * sizeof(treelet_count_pair));
    fclose(data_fd);

    munmap(offsets-1, size*sizeof(uint64_t));
    fclose(offsets_fd);
}

TreeletTable::treelet_count_t TreeletTable::get_count(const long u, const Treelet::treelet_t treelet) const
{
    if( hashes[u]!=NULL ) //There is a perfect hash function for vertex u
    {
        cmph_uint32 id = cmph_search(hashes[u], reinterpret_cast<const char *>(&treelet), sizeof(Treelet::treelet_t));
        treelet_count_pair *pos = data + offsets[u] + id;

        if(pos < data + offsets[u + 1] && treelet == pos->treelet)
            return pos->treelet;
    }
    else
    {
        for(treelet_count_pair* tcp = data + offsets[u]; tcp<data+offsets[u+1]; ++tcp)
        {
            if(tcp->treelet==treelet)
                return tcp->count;
        }
    }

    return 0;
}

TreeletTable::const_iterator TreeletTable::begin(const long u) const
{
    assert(u<size);
    return TreeletTable::const_iterator( data + offsets[u] );
}

TreeletTable::const_iterator TreeletTable::end(const long u) const
{
    assert(u<size);
    return TreeletTable::const_iterator( data + offsets[u+1] );
}
