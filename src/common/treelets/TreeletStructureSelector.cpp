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

#include <vector>
#include <algorithm>
#include <fstream>
#include "TreeletStructureSelector.h"

TreeletStructureSelector TreeletStructureSelector::restrict_to_sizes(const unsigned int from, const unsigned int to_inclusive) const
{
    TreeletStructureSelector result(mode);
    for(const Treelet::treelet_structure_t structure : structures )
        if(unsigned int s = Treelet::number_of_vertices(structure); s>=from && s<=to_inclusive )
            result.structures.insert(structure);

    return result;
}


///Returns a selector suitable to count all the subtrees included by @param this
TreeletStructureSelector TreeletStructureSelector::buildable_closure() const
{
    if(mode==MODE_EXCLUDE)
    {
        unsigned int maxsize = 0;
        for(const auto structure : structures)
            if(unsigned int size = Treelet::number_of_vertices(structure); size>maxsize)
                maxsize = size;

        return restrict_to_sizes(maxsize, maxsize);
    }

    TreeletStructureSelector result(MODE_INCLUDE);
    result.structures.insert(structures.cbegin(), structures.cend());

    std::vector<Treelet::treelet_structure_t> s1(structures.cbegin(), structures.cend());
    std::vector<Treelet::treelet_structure_t> s2;

    auto *to_decompose = &s1;
    auto *next = &s2;

    while(!to_decompose->empty())
    {
        for(const auto structure : *to_decompose)
        {
            Treelet treelet(structure); //FIXME: No need to use treelets

            assert(!treelet.is_singleton());

            Treelet split = Treelet(structure).split_child();
            Treelet complement = treelet.complement(split);

            if(!split.is_singleton())
                if(auto res = result.structures.insert(split.get_structure()); res.second)
                    next->push_back(split.get_structure());

            if(!complement.is_singleton())
                if(auto res = result.structures.insert(complement.get_structure()); res.second)
                    next->push_back(complement.get_structure());
        }

        auto t = to_decompose;
        to_decompose = next;
        next = t;

        next->clear();
    }

    return result;
}

TreeletStructureSelector::TreeletStructureSelector(const std::string &filename)
{
    std::ifstream ifs(filename, std::ifstream::binary);
    if (!ifs.is_open())
        throw std::runtime_error("Could not open file " + filename);

    std::string mode_string;
    ifs >> mode_string;
    if (mode_string == "INCLUDE")
        mode = MODE_INCLUDE;
    else if (mode_string == "EXCLUDE")
        mode = MODE_EXCLUDE;
    else
        throw std::runtime_error("Invalid mode in " + filename);

    Treelet::treelet_structure_t structure;
    while (ifs >> structure)
        structures.insert(structure);
}


Treelet::treelet_structure_t TreeletStructureSelector::star_from_center_structure(const unsigned int size)
{
    Treelet::treelet_structure_t star_from_center = 0; //(1)101010...0
    for(unsigned int i=0; i<size-1; i++)
        star_from_center |= (Treelet::treelet_structure_highest_bit >> (2 * i) );

    return star_from_center;
}

Treelet::treelet_structure_t TreeletStructureSelector::star_from_leaf_structure(const unsigned int size)
{
    Treelet::treelet_structure_t star_from_leaf = Treelet::treelet_structure_highest_bit; //(1)1101010...00
    for(unsigned int i=1; i<size-1; i++)
        star_from_leaf |= (Treelet::treelet_structure_highest_bit >> (2 * i - 1) );

    return star_from_leaf;
}