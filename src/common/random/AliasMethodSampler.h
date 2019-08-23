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

#ifndef MOTIVO_ALIASMETHODSAMPLER_H
#define MOTIVO_ALIASMETHODSAMPLER_H


#include <cstdio>
#include <cstdint>
#include <iostream>
#include <sys/mman.h>
#include <fstream>
#include "Random.h"
#include "../platform/platform.h"

template<typename E, typename W> class AliasMethodSampler
{
private:
    struct entry
    {
        W U;
        E K;
    };

    //FIXME: static_assert(sizeof(entry) == sizeof(W) + sizeof(E), "struct entry is not packed");

    E num_elements;
    W total_weight;
    entry* elements;
    FILE* elements_fd;
    bool readonly;

public:

    AliasMethodSampler(const AliasMethodSampler&) = delete;
    void operator=(const AliasMethodSampler&) = delete;

    explicit AliasMethodSampler(const std::string& filename)
    {
        elements_fd = fopen(filename.c_str(), "rb");

        if(elements_fd==nullptr)
            throw std::runtime_error("Could not open file");

        entry e;
        fread(&e, sizeof(entry), 1, elements_fd);
        num_elements = e.K;
        total_weight = e.U;

        elements = static_cast<entry*>(motivo_mmap_populate((num_elements + 1) * sizeof(entry), PROT_READ, fileno(elements_fd)));
        assert(elements!=MAP_FAILED);
        elements += 1;

        readonly=true;
    }

    explicit AliasMethodSampler(E n) : num_elements(n), total_weight(0), elements_fd(nullptr), readonly(false)
    {
        elements = new entry[num_elements];
        memset(elements, 0, num_elements*sizeof(entry));
    }

    ~AliasMethodSampler()
    {
        if(elements_fd != nullptr)
        {
            motivo_munmap(elements - 1, (num_elements + 1) * sizeof(entry));
            fclose(elements_fd);
        }
        else
            delete[] elements;
    }

    W get_total_weight() const
    {
        return total_weight;
    }

    void set(const E n, const W weight)
    {
        if(readonly)
            throw std::runtime_error("Table is read only");


        total_weight-=elements[n].U;
        elements[n].U=weight;
        //total_weight+=weight;
        safe_add(total_weight, weight, &total_weight);
    }

    void build()
    {
        if(readonly)
            throw std::runtime_error("Table has already been built or is read only");

        E noverfull=0;
        auto overfull = new E[num_elements];
        E nunderfull=0;
        auto underfull = new E[num_elements];

#ifndef NDEBUG
        W of_weight=0;
        W uf_weight=0;
#endif

        for(E i=0; i<num_elements; i++)
        {
            //elements[i].U *= num_elements;
            safe_mul(elements[i].U, num_elements, &elements[i].U);

            // n p_i > 1 <=> n weight_i/tot_weight > 1 <=> n weight_i > tot_weight
            if( elements[i].U > total_weight )
            {
                overfull[noverfull++] = i;
#ifndef NDEBUG
                of_weight += elements[i].U-total_weight;
#endif
            }
            else if( elements[i].U < total_weight )
            {
                underfull[nunderfull++] = i;
#ifndef NDEBUG
                uf_weight += total_weight-elements[i].U;
#endif
            }
        }

        assert(uf_weight <= of_weight);
        assert(uf_weight >= of_weight);

        while(noverfull>0)
        {
            assert(nunderfull>0);
            E of = overfull[--noverfull];
            E uf = underfull[--nunderfull];
            elements[uf].K=of;
            elements[of].U -= total_weight - elements[uf].U;

            if(elements[of].U > total_weight)
                overfull[noverfull++] = of;
            else if(elements[of].U < total_weight )
                underfull[nunderfull++] = of;
        }

        assert(nunderfull==0);

        delete[] overfull;
        delete[] underfull;

        readonly = true;
    }

    inline E sample(Random* rng)
    {
        assert(readonly);
        assert(total_weight>0);

        E i = rng->random_uint<E>(0, num_elements-1);
        W y = rng->random_uint<W>(0, total_weight-1);

        assert(elements[i].U==total_weight || elements[i].K<num_elements);

        return (y<elements[i].U)?i:elements[i].K;
    };

    bool write(const std::string& filename)
    {
        if(!readonly)
            throw std::runtime_error("Table has not been built yet");

        std::ofstream ofs(filename, std::ofstream::binary | std::ofstream::trunc);

        if(ofs.bad())
            return false;

        entry e;
        //Prevent garbage from getting in the file when entry is not aligned (easier debugging)
        memset(&e, 0, sizeof(entry));
        e.K = num_elements;
        e.U = total_weight;

        ofs.write(reinterpret_cast<const char*>(&e), sizeof(entry));
        for(E i=0; i<num_elements; i++)
            ofs.write(reinterpret_cast<const char*>(&elements[i]), sizeof(entry));

        return !ofs.bad();
    }
};


#endif //MOTIVO_ALIASMETHODSAMPLER_H
