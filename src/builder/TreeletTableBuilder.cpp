//
// Created by steven on 11/13/16.
//

#include <vector>
#include <fstream>
#include <cmph.h>
#include <iostream>
#include "TreeletTableBuilder.h"
#include "../bit_cast.h"

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
    }
}

void TreeletTableBuilder::combine(long u, long v)
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
                Treelet t1 = u_it->treelet;
                Treelet t2 = v_it->treelet;

                Treelet merged = t1.merge(t2);

                if(merged.is_valid())
                {
                    assert(u_it->count > 0);
                    assert(v_it->count > 0);
                    assert((u_it->count * v_it->count)/v_it->count == u_it->count);
                    assert((u_it->count * v_it->count)%v_it->count == 0);
                    assert(UINT64_MAX - (*counts[u])[merged] >= u_it->count * v_it->count);
                    (*counts[u])[merged] += u_it->count * v_it->count;
                }
                else if(merged == Treelet::invalid_merge_structure)
                    break; //All the following treelets t2 will have a structure that is too small.
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

    //if(size>1)
        //write_phf(basename);
}

void TreeletTableBuilder::write_data(const std::string &basename) const
{
    std::ofstream offsets(basename + ".off", std::ofstream::binary | std::ofstream::trunc);
    offsets.write(reinterpret_cast<const char*>(&num_vertices), sizeof(uint64_t));

    std::ofstream data(basename + ".dat", std::ofstream::binary | std::ofstream::trunc);
    uint64_t offset=0;
    for(long u=0; u < num_vertices; u++)
    {
        auto tcp = new std::pair<Treelet, TreeletTable::treelet_count_t>[counts[u]->size()];
        std::copy(counts[u]->begin(), counts[u]->end(), tcp);
        std::sort(tcp, tcp+counts[u]->size(), std::greater<std::pair<Treelet, TreeletTable::treelet_count_t>>());

        offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));

        for(unsigned long i=0; i<counts[u]->size(); i++)
        {
            data.write(reinterpret_cast<const char*>(&tcp[i].first), sizeof(Treelet));
            data.write(reinterpret_cast<const char*>(&tcp[i].second), sizeof(TreeletTable::treelet_count_t));
            offset++;
        }

        delete[] tcp;
    }

    offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));
    data.close();
    offsets.close();
    std::cerr << "Written " << offset << " records " << std::endl;
}

cmph_io_adapter_t* TreeletTableBuilder::sparsehash_adapter(const table_t* table)
{
    cmph_io_adapter_t* key_source = new cmph_io_adapter_t;
    key_source->data = new sparsehash_data_t(table);

    assert(table->size()<=INT32_MAX);
    key_source->nkeys = static_cast<cmph_uint32>(table->size());

    key_source->read = key_sparsehash_read;
    key_source->dispose = key_sparsehash_dispose;
    key_source->rewind = key_sparsehash_rewind;

    return  key_source;
}

int TreeletTableBuilder::key_sparsehash_read(void *data, char **key, cmph_uint32 *keylen)
{
    sparsehash_data_t* sh_data = static_cast<sparsehash_data_t*>(data);
    *keylen = sizeof(Treelet);

    //Treelet* t = new Treelet(sh_data->current->first);
    //*key = reinterpret_cast<char*>(t);
    //FIXME: Can we do this? The cmph implementation allocates a new object. However the value of key does not seem
    //to be modified by the implementation.
    *key = const_cast<char*>(reinterpret_cast<const char*>(&sh_data->current->first));
    sh_data->current++;
    return *keylen;
}

void TreeletTableBuilder::key_sparsehash_dispose(void *data, char *key, cmph_uint32 keylen)
{
    (void)data, (void)key, (void)keylen; //suppress unused warnings
    //FIXME: See above
    //delete reinterpret_cast<Treelet*>(key);
}

void TreeletTableBuilder::key_sparsehash_rewind(void *data)
{
    sparsehash_data_t* sh_data = static_cast<sparsehash_data_t*>(data);
    sh_data->current = sh_data->begin;
}

long TreeletTableBuilder::write_phf(const std::string &basename) const
{
    long success = 0;
    unsigned long header_size = (num_vertices+7)/8; //I.e, ceil(num_vertices/8)
    uint8_t* header = new uint8_t[header_size];
    memset(header, 0, header_size);

    FILE* mphf_fd = fopen((basename + ".phf").c_str(), "wb");
    size_t r = fwrite(header, 1, header_size, mphf_fd); //FIXME: fwrite might not write all the data
    assert(r == header_size);

    for(long u=0; u < num_vertices; u++)
    {
        if(counts[u]->size()==0)
            continue;

        cmph_io_adapter_t* source = sparsehash_adapter(counts[u]);
        cmph_config_t *config = cmph_config_new(source);
        cmph_config_set_algo(config, CMPH_CHD);
        cmph_config_set_mphf_fd(config, mphf_fd);
        //cmph_config_set_verbosity(config, 1);
        //cmph_config_set_graphsize(config, 0.50);
        cmph_t* hash = cmph_new(config);
        cmph_config_destroy(config);

        if(hash!=NULL)
        {
            header[u / 8] |= (static_cast<uint8_t>(0b10000000u >> (u%8))); //set the u-th bit in the header
            cmph_dump(hash, mphf_fd);
            cmph_destroy(hash);
            success++;
        }
    }

    fseek(mphf_fd, 0, SEEK_SET);
    r = fwrite(header, 1, header_size, mphf_fd); //FIXME
    assert(r == header_size);
    fclose(mphf_fd);

    delete[] header;

    return success;
}
