//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <iostream>
#include "TreeletTableBuilder.h"

void TreeletTableBuilder::build(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to)
{
    UndirectedGraph::vertex_t num_verts = graph->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
    output->write(reinterpret_cast<const char*>(&from), sizeof(UndirectedGraph::vertex_t));
    output->write(reinterpret_cast<const char*>(&to), sizeof(UndirectedGraph::vertex_t));

    if(size==1)
        do_build_1(from, to);
    else
        do_build(from, to);
}

void TreeletTableBuilder::do_build_1(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to)
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        table_t counts;
        Treelet treelet = Treelet::singleton(coloring->color_of(u));
        counts[treelet] = 1;
        write(counts);
    }
}

void TreeletTableBuilder::do_build(const UndirectedGraph::vertex_t from, const UndirectedGraph::vertex_t to)
{
    for(UndirectedGraph::vertex_t u=from; u<=to; u++)
    {
        table_t counts;
        const UndirectedGraph::vertex_t* neighbors = graph->neighbors(u);
        for(UndirectedGraph::vertex_t d=0; d<graph->degree(u); d++)
            combine(u, neighbors[d], counts);

        normalize(counts);
        write(counts);
    }
}

void TreeletTableBuilder::combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v, table_t& counts)
{
    for(unsigned int size1=1; size1<size; size1++)
    {
        unsigned int size2=size-size1;
        const TreeletTable* u_table = lower->get_table(size1);
        const TreeletTable* v_table = lower->get_table(size2);

        for(TreeletTable::const_iterator u_it = u_table->begin(u); u_it != u_table->end(u); u_it++)
        {
            for(TreeletTable::const_iterator v_it = v_table->begin(v); v_it != v_table->end(v); v_it++)
            {
                const Treelet& t1 = u_it.treelet();
                const Treelet& t2 = v_it.treelet();

                Treelet merged = t1.merge(t2);

                if(merged.is_valid())
                {
                    assert(u_it.count() > 0);
                    assert(v_it.count() > 0);
                    //(*counts[u])[merged] += u_it.count() * v_it.count();

                    TreeletTable::treelet_count_t &count = counts[merged];
                    TreeletTable::treelet_count_t tmp;
                    mul_overflow(u_it.count(), v_it.count(), &tmp);
                    add_overflow(count, tmp, &count);
                }
                else if(merged == Treelet::invalid_merge_structure)
                    break; //All the remaining treelets t2 will have a structure that is too small.
            }
        }
    }
}

void TreeletTableBuilder::normalize(table_t& counts)
{
    for(table_t::iterator u_it = counts.begin(); u_it != counts.end(); u_it++)
    {
        assert(u_it->second % u_it->first.normalization_factor() == 0);
        u_it->second /= u_it->first.normalization_factor();
    }
}

void  TreeletTableBuilder::write(const table_t& counts)
{
    static constexpr TreeletTable::treelet_count_t zero = 0;
    output->write(reinterpret_cast<const char*>(&Treelet::invalid_treelet), sizeof(Treelet));
    output->write(reinterpret_cast<const char*>(&zero), sizeof(TreeletTable::treelet_count_t));

    const uint64_t ntreelets = counts.size();
    auto sorted = new std::pair<Treelet, TreeletTable::treelet_count_t>[ntreelets];
    std::copy(counts.begin(), counts.end(), sorted);
    std::sort(sorted, sorted+ntreelets);

    for(TreeletTable::treelet_count_t i=0; i<ntreelets; i++)
    {
        output->write(reinterpret_cast<const char*>(&sorted[i].first), sizeof(Treelet));
        output->write(reinterpret_cast<const char*>(&sorted[i].second), sizeof(TreeletTable::treelet_count_t));
    }
}

