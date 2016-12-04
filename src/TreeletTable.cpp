//
// Created by steven on 11/20/16.
//

#include "TreeletTable.h"
#include <stdexcept>
#include <sys/mman.h>
#include <algorithm>

TreeletTable::TreeletTable(const std::string& basename)
{
    offsets_fd = fopen( (basename+".off").c_str(), "rb" );
    if(offsets_fd==NULL)
        throw std::runtime_error("Could not open file");

    fread(&size, sizeof(uint64_t), 1, offsets_fd);
    offsets = static_cast<uint64_t*>(mmap(nullptr, (size+1)*sizeof(uint64_t), PROT_READ, MAP_PRIVATE, fileno(offsets_fd), 0));
    assert(offsets!=MAP_FAILED);
    offsets += 1;

    assert(offsets[size] != 0); //FIXME: Handle empty table

    data_fd = fopen( (basename+".dat").c_str(), "rb" );
    data = static_cast<treelet_count_pair*>(mmap(nullptr, offsets[size] * sizeof(treelet_count_pair), PROT_READ, MAP_PRIVATE, fileno(data_fd), 0));
    assert(data!=MAP_FAILED);

    FILE* hashes_fd = fopen( (basename+".phf").c_str(), "rb" );
    if(hashes_fd!=NULL)
    {
        unsigned long header_size = (size+7)/8; //I.e, ceil(size/8)
        uint8_t* hashes_bitmask = new uint8_t[header_size];
        fread(hashes_bitmask, 1, header_size, hashes_fd);

        hashes = new cmph_t*[size];
        for(UndirectedGraph::vertex_t u=0; u<size; u++)
        {
            if( hashes_bitmask[u/8] & (0b10000000u >> u%8) )
                hashes[u] = cmph_load(hashes_fd);
            else
                hashes[u] = nullptr;
        }
        fclose(hashes_fd);
    }
    else
        hashes = nullptr;
}

TreeletTable::~TreeletTable()
{
    if(hashes != nullptr)
    {
        for(UndirectedGraph::vertex_t u = 0; u < size; u++)
            if(hashes[u]!=nullptr)
                cmph_destroy(hashes[u]);

        delete[] hashes;
    }

    munmap(data, size * sizeof(treelet_count_pair));
    fclose(data_fd);

    munmap(offsets-1, size*sizeof(uint64_t));
    fclose(offsets_fd);
}

static const TreeletTable::treelet_count_pair* upper_bound(const TreeletTable::treelet_count_pair* begin, const TreeletTable::treelet_count_pair* end, const Treelet& treelet)
{
    while(begin<end-1)
    {
        const TreeletTable::treelet_count_pair *mid = begin + (end - begin) / 2;
        if(mid->treelet <= treelet)
            begin=mid;
        else
            end=mid;
    }
    return begin;
}

TreeletTable::treelet_count_t TreeletTable::get_count(const UndirectedGraph::vertex_t u, const Treelet treelet) const
{
    if( hashes!=NULL && hashes[u]!=NULL ) //There is a perfect hash function for vertex u
    {
        cmph_uint32 id = cmph_search(hashes[u], reinterpret_cast<const char *>(&treelet), sizeof(Treelet));
        treelet_count_pair *pos = data + offsets[u] + id;

        if(pos < data + offsets[u + 1] && treelet == pos->treelet)
            return pos->count;
    }
    else
    {
        const treelet_count_pair* tcp = upper_bound(data + offsets[u], data+offsets[u+1], treelet);
        if(tcp!=data+offsets[u+1] && tcp->treelet==treelet)
            return tcp->count;
    }

    return 0;
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u) const
{
    assert(u<size);
    return TreeletTable::const_iterator( data + offsets[u] );
}

TreeletTable::const_iterator TreeletTable::end(const UndirectedGraph::vertex_t u) const
{
    assert(u<size);
    return TreeletTable::const_iterator( data + offsets[u+1] );
}

TreeletTable::const_iterator TreeletTable::begin(const UndirectedGraph::vertex_t u, Treelet treelet) const
{
    assert(u<size);
    const treelet_count_pair* tcp = upper_bound(data + offsets[u], data+offsets[u+1], treelet);
    return TreeletTable::const_iterator(tcp);
}

std::pair<Treelet, UndirectedGraph::vertex_t> TreeletTable::get_random_treelet_root_pair(Random* rng) const
{
    uint64_t r = rng->random_uint64(0, offsets[size]);
    UndirectedGraph::vertex_t pos = static_cast<UndirectedGraph::vertex_t>(std::upper_bound(offsets, offsets+size+1, r)-offsets-1);
    return std::make_pair(data[r].treelet, pos); //FIXME: Store root with treelet to make this lookup constant?
}

/*
std::pair<Treelet, long> TreeletTable::get_random_treelet_root_pair()
{
    assert(size>0);
    Random rnd;
    unsigned long r = static_cast<unsigned long>(rnd.random_int32(0, offsets[size])); //FIXME: Large number of treelets

    unsigned long ub, lb;
    ub = lb = r/size; //Get an estimate of the right position

    while(r < offsets[lb])
    {
        ub=lb;
        lb=lb/2;
    }

    while(r >= offsets[ub])
    {
        lb=ub;
        ub=(ub*2 <= offsets[size])?(ub*2):size;
    }

    assert(lb<ub);
    while(lb<ub-1)
    {
        unsigned long mid = (ub-lb)/2;
        if(r < offsets[mid])
            ub=mid;
        else
            lb=mid;
    }

    assert(lb<ub);

    return std::make_pair(data[r].treelet, lb);
}*/

