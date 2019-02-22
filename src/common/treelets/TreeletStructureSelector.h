//
// Created by steven on 2/3/19.
//

#ifndef MOTIVO_TREELETSTRUCTURESELECTOR_H
#define MOTIVO_TREELETSTRUCTURESELECTOR_H

#include <set>
#include "Treelet.h"

class TreeletStructureSelector
{
public:
    typedef std::set<Treelet::treelet_structure_t>::const_iterator const_iterator;

    typedef int mode_t;
    constexpr static int MODE_INCLUDE = 1;
    constexpr static int MODE_EXCLUDE = 2;

private:
    mode_t mode;
    std::set<Treelet::treelet_structure_t> structures;

    explicit TreeletStructureSelector(const mode_t mode) noexcept : mode(mode)
    {}

public:
    mode_t get_mode() const { return mode; }

    template<class InputIt> TreeletStructureSelector(const mode_t mode, InputIt first, InputIt last) : mode(mode)
    {
        structures.insert(first, last);
    }

    explicit TreeletStructureSelector(const std::string& filename);


    uint64_t size() const { return structures.size(); }

    bool is_included(const Treelet::treelet_structure_t structure) const { return structures.count(structure)!=0; }

    TreeletStructureSelector restrict_to_sizes(unsigned int from, unsigned int to_inclusive) const;

    TreeletStructureSelector intersection(const TreeletStructureSelector &other) const;

    TreeletStructureSelector buildable_closure() const;

    const_iterator begin() const { return structures.cbegin(); }

    const_iterator end() const { return structures.cend(); }
};


#endif //MOTIVO_TREELETSTRUCTURESELECTOR_H
