//
// Created by steven on 11/13/16.
//

#include <fstream>
#include <cmph.h>
#include "TreeletTableBuilder.h"

TreeletTableBuilder::TreeletTableBuilder(Graph* graph, GraphColoring* coloring, int size, const TreeletTableCollection* lower)
        :  graph(graph), num_vertices(graph->number_of_vertices()), coloring(coloring), size(size), lower(lower)
{
    counts = new table_t*[num_vertices];
    for(long u=0; u<num_vertices; u++)
    {
        counts[u] = new table_t();
        counts[u]->set_deleted_key(0); //0 is an invalid treelet
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
        Treelet::treelet_t treelet = Treelet::singleton(coloring->color_of(u));
        (*counts[u])[treelet] = 1;
    }
}

void TreeletTableBuilder::do_fill()
{
    for(long u=0; u<num_vertices; u++)
    {
        const long* neighbors = graph->neighbors(u);
        for(long d=0; d<graph->degree(u); d++)
            combine(u, neighbors[d]);

        normalize(u);
        counts[u]->resize(0); //Reduce to the smallest size
    }
}

void TreeletTableBuilder::combine(long u, long v)
{
    for(int size1=1; size1<size; size1++)
    {
        int size2=size-size1;
        const TreeletTable* u_table = lower->get_table(size1);
        const TreeletTable* v_table = lower->get_table(size2);

        for(TreeletTable::const_iterator u_it = u_table->begin(u); u_it != u_table->end(u); u_it++)
        {
            for(TreeletTable::const_iterator v_it = v_table->begin(v); v_it != v_table->end(v); v_it++)
            {
                Treelet::treelet_t t1 = u_it->treelet;
                Treelet::treelet_t t2 = v_it->treelet;

                Treelet::treelet_t merged = Treelet::merge(t1, t2);

                if( merged != Treelet::invalid_treelet )
                {
                    assert(u_it->count > 0);
                    assert(v_it->count > 0);
                    assert((u_it->count * v_it->count)/v_it->count == u_it->count);
                    assert((u_it->count * v_it->count)%v_it->count == 0);
                    assert(UINT64_MAX - (*counts[u])[merged] >= u_it->count * v_it->count);
                    (*counts[u])[merged] += u_it->count * v_it->count;
                }
            }
        }
    }
}

void TreeletTableBuilder::normalize(long u)
{
    for(table_t::iterator u_it = counts[u]->begin(); u_it != counts[u]->end(); u_it++)
    {
        assert(u_it->second % Treelet::normalization_factor(u_it->first) == 0);
        u_it->second /= Treelet::normalization_factor(u_it->first);
    }
}

void TreeletTableBuilder::write(const std::string &basename) const
{
    write_data(basename);

    if(size>1)
        write_phf(basename);
}

void TreeletTableBuilder::write_data(const std::string &basename) const
{
    std::ofstream offsets(basename + ".off", std::ofstream::binary | std::ofstream::trunc);
    offsets.write(reinterpret_cast<const char*>(&num_vertices), sizeof(uint64_t));

    std::ofstream data(basename + ".dat", std::ofstream::binary | std::ofstream::trunc);
    uint64_t offset=0;
    for(long u=0; u < num_vertices; u++)
    {
        offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));
        for(table_t::iterator it = counts[u]->begin(); it != counts[u]->end(); it++)
        {
            data.write(reinterpret_cast<const char*>(&it->first), sizeof(Treelet::treelet_t));
            data.write(reinterpret_cast<const char*>(&it->second), sizeof(TreeletTable::treelet_count_t));

            offset++;
        }
    }
    offsets.write(reinterpret_cast<const char*>(&offset), sizeof(uint64_t));
    data.close();
    offsets.close();
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
    *keylen = sizeof(table_t::key_type);

    //*key = reinterpret_cast<char*>(new table_t::key_type(sh_data->current->first));
    //FIXME: Can we do this? The cmph implementation allocates a new object. However the values of key does not seem
    //to be modified by the implementation.
    *key = const_cast<char*>(reinterpret_cast<const char*>(&sh_data->current->first));
    sh_data->current++;
    return *keylen;
}

void TreeletTableBuilder::key_sparsehash_dispose(void *data, char *key, cmph_uint32 keylen)
{
    (void)data, (void)key, (void)keylen; //suppress unused warnings

    //FIXME: See above
    //delete reinterpret_cast<table_t::key_type*>(key);
}

void TreeletTableBuilder::key_sparsehash_rewind(void *data)
{
    sparsehash_data_t* sh_data = static_cast<sparsehash_data_t*>(data);
    sh_data->current = sh_data->begin;
}

long TreeletTableBuilder::write_phf(const std::string &basename) const
{
    long success = 0;
    long header_size = (num_vertices+7)/8; //I.e, ceil(num_vertices/8)
    uint8_t header[header_size] = {0};

    FILE* mphf_fd = fopen((basename + ".phf").c_str(), "wb");
    int r = fwrite(header, 1, header_size, mphf_fd); //FIXME: fwrite might not write all the data
    assert(r == header_size);

    for(long u=0; u < num_vertices; u++)
    {
        cmph_io_adapter_t* source = sparsehash_adapter(counts[0]);
        cmph_config_t *config = cmph_config_new(source);
        cmph_config_set_algo(config, CMPH_CHD);
        cmph_config_set_mphf_fd(config, mphf_fd);
        //cmph_config_set_verbosity(config, 1);
        //cmph_config_set_graphsize(config, 0.50);
        cmph_t* hash = cmph_new(config);
        cmph_config_destroy(config);

        if(hash!=NULL)
        {
            header[u / 8] |= (0b10000000u >> u % 8); //set the u-th bit in the header
            cmph_dump(hash, mphf_fd);
            cmph_destroy(hash);
            success++;
        }
    }

    fseek(mphf_fd, 0, SEEK_SET);
    r = fwrite(header, 1, header_size, mphf_fd); //FIXME
    assert(r == header_size);
    fclose(mphf_fd);

    return success;
}
