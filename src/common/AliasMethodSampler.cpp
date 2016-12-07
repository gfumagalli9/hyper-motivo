//
// Created by steven on 12/6/16.
//

#include <cassert>
#include <cstring>
#include <sys/mman.h>
#include <stdexcept>
#include <fstream>
#include "AliasMethodSampler.h"
#include "../platform.h"

AliasMethodSampler::AliasMethodSampler(uint64_t n) : num_elements(n), total_weight(0), elements_fd(NULL), readonly(false)
{
    elements = new element[num_elements];
    memset(elements, 0, num_elements*sizeof(element));
}

AliasMethodSampler::AliasMethodSampler(const std::string &filename)
{
    elements_fd = fopen(filename.c_str(), "rb");

    if(elements_fd==NULL)
        throw std::runtime_error("Could not open file");

    fread(&num_elements, sizeof(uint64_t), 1, elements_fd);
    fread(&total_weight, sizeof(uint64_t), 1, elements_fd);

    elements = static_cast<element*>(mmap(NULL, (num_elements+1)*sizeof(uint64_t), PROT_READ, MAP_PRIVATE, fileno(elements_fd), 0));
    assert(elements!=MAP_FAILED);
    elements += 1;

    readonly=true;
}


AliasMethodSampler::~AliasMethodSampler()
{
    if(elements_fd != NULL)
    {
        munmap(elements - 1, (num_elements + 1) * sizeof(uint64_t));
        fclose(elements_fd);
    }
    else
        delete[] elements;
}

void AliasMethodSampler::build()
{
    if(readonly)
        throw std::runtime_error("Table has already been built or is read only");

    uint64_t noverfull=0;
    uint64_t* overfull = new uint64_t[num_elements];
    uint64_t nunderfull=0;
    uint64_t* underfull = new uint64_t[num_elements];
    for(uint64_t i=0; i<num_elements; i++)
    {
        //elements[i].U *= num_elements;
        mul_overflow(elements[i].U, num_elements, &elements[i].U);

        // n p_i > 1 <=> n weight_i/tot_weight > 1 <=> n weight_i > tot_weight
        if( elements[i].U > total_weight )
            overfull[noverfull++]=i;
        else if( elements[i].U < total_weight )
            underfull[nunderfull++]=i;
    }

    while(noverfull>0)
    {
        assert(nunderfull>0);
        uint64_t of = overfull[--noverfull];
        uint64_t uf = underfull[--nunderfull];
        elements[uf].K=of;
        elements[of].U-=total_weight-elements[uf].U;

        if(elements[of].U > total_weight)
            overfull[noverfull++]=of;
        else if(elements[of].U < total_weight )
            underfull[nunderfull++]=of;
    }

    assert(nunderfull==0);

    delete[] overfull;
    delete[] underfull;

    readonly = true;
}

bool AliasMethodSampler::write(const std::string& filename)
{
    if(!readonly)
        throw std::runtime_error("Table has not been built yet");

    std::ofstream ofs(filename, std::ofstream::binary | std::ofstream::trunc);

    if(ofs.bad())
        return false;

    ofs.write(reinterpret_cast<const char*>(&num_elements), sizeof(uint64_t));
    ofs.write(reinterpret_cast<const char*>(&total_weight), sizeof(uint64_t));
    for(uint64_t i=0; i<num_elements; i++)
    {
        ofs.write(reinterpret_cast<const char*>(&elements[i]), sizeof(element));
    }

    return !ofs.bad();
}

void AliasMethodSampler::set(const uint64_t n, const uint64_t weight)
{
    if(readonly)
        throw std::runtime_error("Table is read only");


    total_weight-=elements[n].U;
    elements[n].U=weight;
    //total_weight+=weight;
    add_overflow(total_weight, weight, &total_weight);
}

