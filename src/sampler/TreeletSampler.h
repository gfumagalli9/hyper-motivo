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

#ifndef MOTIVO_TREELETSAMPLER_H
#define MOTIVO_TREELETSAMPLER_H

#include <atomic>
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "DynamicSequencer.h"

class TreeletSampler
{
private:
    class DecompositionFIFOBuffer
    {
    private:
        uint32_t size=0;
        uint32_t capacity;
        std::pair<UndirectedGraph::vertex_t, Treelet> *entries;

    public:
        explicit DecompositionFIFOBuffer(uint32_t capacity) : capacity(capacity)
        {
            entries = new std::pair<UndirectedGraph::vertex_t, Treelet>[capacity];
        };

        ~DecompositionFIFOBuffer()
        {
            delete[] entries;
        }

        DecompositionFIFOBuffer(const DecompositionFIFOBuffer&) = delete;

        void push(UndirectedGraph::vertex_t v, const Treelet& t)
        {
            assert(size<capacity);
            entries[size].first = v;
            entries[size].second = t;
            size++;
        }

        std::pair<UndirectedGraph::vertex_t, Treelet> pop()
        {
            assert(size>0);
            return entries[--size];
        }

        bool empty() const { return size==0; }
    };

    const UndirectedGraph* graph;
    const TreeletTableCollection* table_collection;
    const unsigned int size;
    const uint32_t buffer_size;
    const UndirectedGraph::vertex_t buffer_degree;

    bool use_selector = false;
    RangeSampler<TreeletTable::treelet_count_t>** range_samplers = nullptr;
    AliasMethodSampler<UndirectedGraph::vertex_t,TreeletTable::treelet_count_t>* root_sampler = nullptr;

    void populate_root_and_range_sampler_mt(DynamicSequencer<UndirectedGraph::vertex_t> &sequencer, const TreeletStructureSelector *selector);

    void populate_buffer(DecompositionFIFOBuffer &buffer, UndirectedGraph::vertex_t u, const Treelet& t, Random *rng);

public:
    TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, unsigned int size, uint32_t buffer_size, UndirectedGraph::vertex_t buffer_degree);
    ~TreeletSampler();

    ///Samples an occurrence of @param t rooted in @param u
    bool sample_rooted_occurrence [[gnu::hot]] (const Treelet& t, UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence, Random *rng);

    UndirectedGraph::vertex_t sample_root [[gnu::hot]] (Random* rng)
    {
        if(use_selector)
            return root_sampler->sample(rng);
	else
            return table_collection->get_table(size)->get_random_root(rng);
    }

    Treelet sample_treelet [[gnu::hot]] (UndirectedGraph::vertex_t root, Random* rng)
    {
        if(use_selector)
        {
            Treelet t = table_collection->get_table(size)->get_treelet_no(root, range_samplers[root]->sample(rng));
            assert(t.is_valid());
            return t;
        }
	else
        {
            Treelet t = table_collection->get_table(size)->get_random_treelet(root, rng);
            assert(t.is_valid());
            return t;
        }
    }

    void set_selector(const TreeletStructureSelector *selector, unsigned int nthreads);
};


#endif //MOTIVO_TREELETSAMPLER_H
