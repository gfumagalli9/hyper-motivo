//
// Created by steven on 11/13/16.
//

#include "TreeletTable.h"

TreeletTable::TreeletTable(Graph* graph, GraphColoring* coloring, int size, const TreeletTable **lower)
        : coloring(coloring), graph(graph), size(size), lower(lower)
{
    counts = new table_t*[graph->number_of_vertices()];
    for(long u=0; u<graph->number_of_vertices(); u++)
    {
        counts[u] = new table_t();
        counts[u]->set_deleted_key(0); //0 is an invalid treelet
    }
}

TreeletTable::~TreeletTable()
{
    for(long u=0; u< graph->number_of_vertices(); u++)
        delete[] counts[u];

    delete[] counts;
}

void TreeletTable::fill_table()
{
    if(size==1)
        do_fill_table_1();
    else
        do_fill_table();
}

void TreeletTable::do_fill_table_1()
{
    for(long u=0; u< graph->number_of_vertices(); u++)
    {
        Treelet::treelet_t treelet = Treelet::singleton(coloring->color_of(u));
        (*counts[u])[treelet] = 1;
    }
}

void TreeletTable::do_fill_table()
{
    for(long u=0; u< graph->number_of_vertices(); u++)
    {
        const long* neighbors = graph->neighbors(u);
        for(long d=0; d<graph->degree(u); d++)
            combine(u, neighbors[d]);

        normalize(u);
    }
}

void TreeletTable::combine(long u, long v)
{
    for(int size1=1; size1<size; size1++)
    {
        int size2=size-size1;
        const table_t* u_table = lower[size1-1]->counts[u];
        const table_t* v_table = lower[size2-1]->counts[v];

        for(table_t::const_iterator u_it = u_table->begin(); u_it != u_table->end(); u_it++)
        {
            for(table_t::const_iterator v_it = v_table->begin(); v_it != v_table->end(); v_it++)
            {
                Treelet::treelet_t t1 = u_it->first;
                Treelet::treelet_t t2 = v_it->first;

                if( Treelet::is_mergeable(t1, t2) )
                {
                    Treelet::treelet_t merged = Treelet::merge(t1, t2);
                    (*counts[u])[merged] +=  u_it->second * v_it->second;
                }
            }
        }
    }
}

void TreeletTable::normalize(long u)
{
    for(table_t::iterator u_it = counts[u]->begin(); u_it != counts[u]->end(); u_it++)
        u_it->second /= Treelet::normalization_factor(u_it->first);
}


