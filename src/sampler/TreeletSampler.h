//
// Created by steven on 11/27/16.
//

#ifndef MOTIVO_TREELETSAMPLER_H
#define MOTIVO_TREELETSAMPLER_H

#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTableCollection.h"
#include "../common/treelets/TreeletSelector.h"
#include "../common/sequencer/DynamicSequencer.h"

class TreeletSampler
{
private:
    const UndirectedGraph* graph;
    const TreeletTableCollection* table_collection;
    const unsigned int size;

    const TreeletSelector* selector = nullptr;
    RangeSampler<TreeletTable::treelet_count_t>** range_samplers = nullptr;
    AliasMethodSampler<UndirectedGraph::vertex_t,TreeletTable::treelet_count_t>* root_sampler = nullptr;

    void populate_root_and_range_sampler_mt(DynamicSequencer<UndirectedGraph::vertex_t>* sequencer);

public:
    TreeletSampler(const UndirectedGraph *graph, const TreeletTableCollection *ttc, const unsigned int size);
    ~TreeletSampler();

    ///Samples an occurrence of @param t rooted in @param u
    bool sample_rooted_occurrence [[gnu::hot]] (const Treelet& t, const UndirectedGraph::vertex_t u, UndirectedGraph::vertex_t* occurrence, Random *rng);

    UndirectedGraph::vertex_t sample_root [[gnu::hot]] (Random* rng)
    {
        if(!selector)
            return table_collection->get_table(size)->get_random_root(rng);
        else
            return root_sampler->sample(rng);
    }

    Treelet sample_treelet [[gnu::hot]] (UndirectedGraph::vertex_t root, Random* rng)
    {
        if(!selector)
        {
            Treelet t = table_collection->get_table(size)->get_random_treelet(root, rng);
            assert(t.is_valid());
            return t;
        }
        else
        {
            Treelet t = table_collection->get_table(size)->get_treelet_no(root, range_samplers[root]->sample(rng));
            assert(t.is_valid());
            assert(selector->is_included(t));
            return t;
        }
    }

    void set_selector(const TreeletSelector *selector, unsigned int nthreads);
};


#endif //MOTIVO_TREELETSAMPLER_H
