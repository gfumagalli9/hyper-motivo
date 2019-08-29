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

#ifndef MOTIVO_TREELETSTRUCTURESELECTOR_H
#define MOTIVO_TREELETSTRUCTURESELECTOR_H

#include <set>
#include "Treelet.h"

class TreeletStructureSelector
{
public:
    typedef int mode_t;
    constexpr static int MODE_INCLUDE = 1;
    constexpr static int MODE_EXCLUDE = 2;

private:
    typedef std::set<Treelet::treelet_structure_t, std::greater<> > treelet_structure_set_t;
    mode_t mode;
    treelet_structure_set_t structures;

public:
    //Keep structures in decreasing order, to be consistent with treelet ordering
    typedef treelet_structure_set_t::const_iterator const_iterator;

    static Treelet::treelet_structure_t star_from_center_structure(unsigned int size);

    static Treelet::treelet_structure_t star_from_leaf_structure(unsigned int size);

    explicit TreeletStructureSelector(const mode_t mode) noexcept : mode(mode)
    {}

    mode_t get_mode() const { return mode; }

    template<class InputIt> TreeletStructureSelector(const mode_t mode, InputIt first, InputIt last) : mode(mode)
    {
        structures.insert(first, last);
    }

    explicit TreeletStructureSelector(const std::string& filename);

    uint64_t size() const { return structures.size(); }

    bool is_included(const Treelet::treelet_structure_t structure) const { return (structures.count(structure)!=0)==(mode==MODE_INCLUDE); }

    TreeletStructureSelector restrict_to_sizes(unsigned int from, unsigned int to_inclusive) const;

    TreeletStructureSelector buildable_closure() const;

    const_iterator begin() const { return structures.cbegin(); }

    const_iterator end() const { return structures.cend(); }

    void add_structure_with_current_mode(Treelet::treelet_structure_t structure)
    {
        structures.insert(structure);
    }
};


#endif //MOTIVO_TREELETSTRUCTURESELECTOR_H
