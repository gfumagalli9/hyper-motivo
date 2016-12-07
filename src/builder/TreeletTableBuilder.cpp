//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <iostream>
#include "TreeletTableBuilder.h"
#include "../common/AliasMethodSampler.h"

TreeletTableBuilder::TreeletTableBuilder(UndirectedGraph* graph, GraphColoring* coloring, const unsigned int size, const TreeletTableCollection* lower)
        :  graph(graph), num_vertices(graph->number_of_vertices()), coloring(coloring), size(size), lower(lower)
{
    counts = new table_t*[num_vertices];
    for(long u=0; u<num_vertices; u++)
    {
        counts[u] = new table_t();
        //counts[u]->set_deleted_key(Treelet::invalid_treelet);
    }
}

TreeletTableBuilder::~TreeletTableBuilder()
{
    for(long u=0; u<num_vertices; u++)
        delete counts[u];

    delete[] counts;
}

void TreeletTableBuilder::build()
{
    if(size==1)
        do_fill_1();
    else
        do_fill();
}

void TreeletTableBuilder::do_fill_1()
{
    for(long u=0; u<num_vertices; u++)
    {
        Treelet treelet = Treelet::singleton(coloring->color_of(u));
        (*counts[u])[treelet] = 1;
    }
}

void TreeletTableBuilder::do_fill()
{
    std::cerr << "Num verts: " <<num_vertices << std::endl;
    for(UndirectedGraph::vertex_t u=0; u<num_vertices; u++)
    {
        const UndirectedGraph::vertex_t* neighbors = graph->neighbors(u);
        for(UndirectedGraph::vertex_t d=0; d<graph->degree(u); d++)
            combine(u, neighbors[d]);

        normalize(u);
        counts[u]->resize(0); //Reduce to the smallest size
        //FIXME: Save now and keep only one hashtable to save on memory?
    }
}

void TreeletTableBuilder::combine(const UndirectedGraph::vertex_t u, const UndirectedGraph::vertex_t v)
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

                    TreeletTable::treelet_count_t &count = (*counts[u])[merged];
                    TreeletTable::treelet_count_t tmp;
                    mul_overflow(u_it.count(), v_it.count(), &tmp);
                    add_overflow(count, tmp, &count);
                }
                else if(merged == Treelet::invalid_merge_structure)
                    break; //All the following treelets t2 will have a structure that is big small.
            }
        }
    }
}

void TreeletTableBuilder::normalize(long u)
{
    for(table_t::iterator u_it = counts[u]->begin(); u_it != counts[u]->end(); u_it++)
    {
        assert(u_it->second % u_it->first.normalization_factor() == 0);
        u_it->second /= u_it->first.normalization_factor();
    }
}

void TreeletTableBuilder::write(const std::string &basename) const
{
    write_data(basename);

    //if(num_vertices>1)
        //write_phf(basename);
}

void TreeletTableBuilder::write_data(const std::string &basename) const
{
    std::ofstream offsets(basename + ".off", std::ofstream::binary | std::ofstream::trunc);
    offsets.write(reinterpret_cast<const char*>(&num_vertices), sizeof(uint64_t));

    std::ofstream data(basename + ".dat", std::ofstream::binary | std::ofstream::trunc);
    uint64_t offset=0;

    AliasMethodSampler root_sampler(num_vertices);

    TreeletTable::treelet_count_t num_treelets = 0;
    data.write(reinterpret_cast<const char*>(&Treelet::invalid_treelet), sizeof(Treelet));
    data.write(reinterpret_cast<const char*>(&num_treelets), sizeof(TreeletTable::treelet_count_t));
    for(UndirectedGraph::vertex_t u=0; u < num_vertices; u++)
    {
        auto tcp = new std::pair<Treelet, TreeletTable::treelet_count_t>[counts[u]->size()];
        std::copy(counts[u]->begin(), counts[u]->end(), tcp);
        std::sort(tcp, tcp+counts[u]->size());

        offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));
        for(uint64_t i=0; i<counts[u]->size(); i++)
        {
            //num_treelets += tcp[i].second;
            add_overflow(num_treelets, tcp[i].second, &num_treelets);
            data.write(reinterpret_cast<const char*>(&tcp[i].first), sizeof(Treelet));
            data.write(reinterpret_cast<const char*>(&num_treelets), sizeof(TreeletTable::treelet_count_t));

        }
        offset+=counts[u]->size();
        root_sampler.set(u, counts[u]->size());

        delete[] tcp;
    }

    offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));
    data.close();
    offsets.close();
    std::cerr << "Written " << offset << " records " << std::endl;

    root_sampler.build();
    root_sampler.write(basename+".rts");
}

