//
// Created by steven on 2/3/19.
//

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

            if(treelet.is_singleton())
                continue;

            Treelet split = Treelet(structure).split_child();
            Treelet complement = treelet.complement(split);

            if(auto res = result.structures.insert(split.get_structure()); res.second)
                next->push_back(split.get_structure());

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

TreeletStructureSelector TreeletStructureSelector::intersection(const TreeletStructureSelector &other) const
{
    if(mode == MODE_EXCLUDE && other.mode == MODE_EXCLUDE)
    {
        TreeletStructureSelector result(MODE_EXCLUDE);
        result.structures.insert(structures.cbegin(), structures.cend());
        result.structures.insert(other.structures.cbegin(), other.structures.cend());

        return result;
    }

    TreeletStructureSelector result(MODE_INCLUDE);
    const TreeletStructureSelector &include_selector = (mode==MODE_INCLUDE)?(*this):(other);
    const TreeletStructureSelector &other_selector = (mode==MODE_INCLUDE)?(other):(*this);
    for(Treelet::treelet_structure_t structure : include_selector.structures )
        if(other_selector.is_included(structure))
            result.structures.insert(structure);

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


