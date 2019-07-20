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

#include "ColorCodingSpanningTreeCounter.h"

ColorCodingSpanningTreeCounter::ColorCodingSpanningTreeCounter(const Occurrence *occurrence, bool store_only_0, const TreeletStructureSelector *selector)
        : occurrence(occurrence), size(occurrence->get_size()), store_only_0(store_only_0), selector(selector)
{
    if(!occurrence->is_valid())
        throw std::runtime_error("Invalid occurrence");
}

ColorCodingSpanningTreeCounter::~ColorCodingSpanningTreeCounter()
{
    if(tables==nullptr)
        return;

    for(unsigned int  i = 1; i<=size; i++)
        delete[] tables[i-1];

    delete[] tables;
}


void ColorCodingSpanningTreeCounter::count()
{
    if(tables!=nullptr)
        return;

    //tables[s-1][i] contains the table of size s for vertex i
    tables = new table_t*[size];

    //Fill table of size 1
    tables[0] = new table_t[size];
    for(uint8_t u = 0; u < size; u++)
    {
        COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[0][u]);
        tables[0][u][Treelet::singleton(u)] = 1;
    }

    //Fill tables of sizes 2,...,size
    for(unsigned int i = 2; i<=size; i++)
    {
        tables[i-1] = new table_t[size];
        do_build(i);
    }
}



void ColorCodingSpanningTreeCounter::do_build(const unsigned int current_size)
{
    if(!store_only_0 || current_size != size)
    {
        for (unsigned int u = 0; u < occurrence->get_size(); u++)
        {
            COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[current_size - 1][u]);

            for (unsigned int v = 0; v < u; v++)
            {
                if (occurrence->has_edge(u, v)) //u > v must hold
                {
                    combine(u, v, current_size);
                    combine(v, u, current_size);
                }
            }
        }
    }
    else
    {
        COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[current_size - 1][0]);
        for (unsigned int v=1; v<size; v++)
        {
            COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[current_size - 1][v]);
            if (occurrence->has_edge(v, 0))
                combine(0, v, current_size);
        }
    }

    //Normalization
    //Alternatively we could also keep a vector of 'TreeletCountPair's for each size and vertex
    //(as opposed to one hashtable per size per vertex)
    //In this case a single hashtable would be used to count the occurrences of treelets for a single vertex
    //Then its contents are copied to a vector (and normalized) and sorted.
    //This requires more computational effort but allows to break early in combine()
    for(unsigned int u = 0; u < size; u++)
    {
        for (auto &entry : tables[current_size-1][u])
        {
            assert(entry.second % entry.first.normalization_factor() == 0);
            entry.second /= entry.first.normalization_factor();
        }
    }
}




void ColorCodingSpanningTreeCounter::combine(const unsigned int u, const unsigned int v, const unsigned int current_size)
{
    for(unsigned int size1 = 1; size1 < current_size; size1++)
    {
        for (const auto &entry1 : tables[size1-1][u])
        {
            assert(entry1.first.is_valid());
            assert(entry1.second != 0);

            for (const auto &entry2 : tables[current_size-size1-1][v])
            {
                assert(entry1.first.is_valid());
                assert(entry2.second != 0);

                Treelet merged = entry1.first.merge(entry2.first);
                if (!merged.is_valid() || (selector && !selector->is_included(merged.get_structure())))
                    continue; //We could break early in case of invalid merge if we use a sorted vector (like in TreeletTableBuilder)

                tables[current_size-1][u][merged] += entry1.second * entry2.second;
            }
        }
    }
}


/**
 * The total number of *rooted* spanning trees of this graphlet; that is, k times the number of distinct spanning trees.
 */
uint64_t ColorCodingSpanningTreeCounter::number_of_counted_rooted_spanning_trees() const
{
    uint64_t spanning_trees = 0;
    for(unsigned int u = 0; u < occurrence->get_size(); u++)
        for (auto &entry : tables[size-1][u])
            spanning_trees += entry.second;

    return spanning_trees;
}

/**
 * The total number of spanning trees of this graphlet *rooted* at a given node.
 * Due to TreeletSelector, in this is not simply the number of spanning trees divided by k.
 */
uint64_t ColorCodingSpanningTreeCounter::number_of_counted_spanning_trees_rooted_at(const unsigned int root) const
{
    uint64_t spanning_trees = 0;
    for (auto &entry : tables[size-1][root])
            spanning_trees += entry.second;

    return spanning_trees;
}



