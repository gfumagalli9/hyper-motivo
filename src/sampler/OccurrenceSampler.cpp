//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include "OccurrenceSampler.h"

void OccurrenceSampler::sample_one(Occurrence *occurrence)
{
    UndirectedGraph::vertex_t sampled_vertices[16];
    UndirectedGraph::vertex_t root = sampler.sample_root(size);
    assert(root<graph->number_of_vertices());
    Treelet t = sampler.sample_treelet(size, root);

    while(true)
    {
        if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
        {
#ifndef NDEBUG
            bool success =
#endif
                    sampler.sample_rooted_occurrence(t, root, sampled_vertices);
            assert(success);
        }

        if (graphlets)
        {
            new (occurrence) Occurrence(size, graph, sampled_vertices);

            if (!no_rejection && rng->random_uint<uint64_t>(0,  occurrence->number_of_spanning_trees() - 1) != 0)
                continue; //Rejection
        }
        else
            new (occurrence) Occurrence(t, sampled_vertices);

        break;
    }

    if(canonicize)
        occurrence->canonicize();
}

void OccurrenceSampler::sample()
{
    std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();

    if(number_of_threads==1)
        do_sample_st();
    else
    {
        sequencer_t *sequencer = new sequencer_t(1, num_samples, number_of_threads);
        ConcurrentWriter* writer = new ConcurrentWriter(output, 10*number_of_threads);

        std::thread *worker_threads = new std::thread[number_of_threads];
        for(unsigned int i = 0; i < number_of_threads; i++)
            worker_threads[i] = std::thread([this, sequencer, writer] { do_sample_mt(sequencer, writer); });

        for(unsigned int i = 0; i < number_of_threads; i++)
            worker_threads[i].join();

        delete[] worker_threads;
        delete writer;
        delete sequencer;
    }

    std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

    std::cerr << "Sampling time: " << delta_t.count() << " s\n";
    //std::cerr << "Sampled treelets: " << sampled << " (" << static_cast<double>(sampled)/delta_t.count() << " occ/s)" << "\n";
    //std::cerr << "Accepted treelets/graphlets: " << accepted<< " (" << static_cast<double>(accepted)/delta_t.count() << " occ/s)" << "\n";
    //std::cerr << "Rejected treelets/graphlets: " << sampled - accepted << std::endl;
}


void OccurrenceSampler::do_sample_st()
{
    Occurrence occurrence;
    char* buffer = new char[buffer_size];
    char* p=buffer;

    for(uint64_t i=0; i<num_samples; i++)
    {
        sample_one(&occurrence);
        if(p-buffer < max_occurrence_size)
        {
            output->write(buffer, p-buffer);
            p = buffer = new char[buffer_size];
        }

        p=write(&occurrence, p);
    }

    if(p!=buffer)
        output->write(buffer, p-buffer);
    else
        delete[] buffer;
}

void OccurrenceSampler::do_sample_mt(sequencer_t *sequencer, ConcurrentWriter *writer)
{
    Occurrence occurrence;
    char* buffer = new char[buffer_size];
    char* p=buffer;

    while(true)
    {
        sequencer_t::sequence_batch_t batch = sequencer->next_batch();
        if(batch.to<=batch.from)
            break;

        for (uint64_t i = batch.from; i < batch.to; i++)
        {
            sample_one(&occurrence);

            if (p - buffer < max_occurrence_size)
            {
                writer->write(buffer, static_cast<std::size_t>(p - buffer));
                p = buffer = new char[buffer_size];
            }

            p = write(&occurrence, p);
        }
    }

    if(p!=buffer)
        writer->write(buffer, static_cast<std::size_t>(p-buffer));
    else
        delete[] buffer;
}

char* OccurrenceSampler::write(Occurrence *occurrence, char* buf)
{
    if(text)
    {
        if(footprints)
        {
            strcpy(buf, occurrence->text_footprint());
            buf += Occurrence::text_footprint_bytes;
            *(buf++) = ';';
        }

        if(spanning_trees_no)
            buf += sprintf(buf, "%" PRIu64 ";", occurrence->number_of_spanning_trees());

        if(vertices)
        {
            const UndirectedGraph::vertex_t* verts = occurrence->vertices();
            for(unsigned int i=0; i < size; i++)
            {
                buf += sprintf(buf, "%" PRIu32, verts[i]);
                (*buf++) = (i == size - 1) ? ';' : ' ';
            }
        }

        (*buf++) = '\n';
    }
    else
    {
        if(footprints)
        {
            memcpy(buf, occurrence->binary_footprint(), Occurrence::binary_footprint_bytes);
            buf+=Occurrence::binary_footprint_bytes;
        }

        if(spanning_trees_no)
        {
            uint64_t st = occurrence->number_of_spanning_trees();
            memcpy(buf, &st, sizeof(uint64_t));
            buf+=sizeof(uint64_t);
        }

        if(vertices)
        {
            memcpy(buf, occurrence->vertices(), sizeof(UndirectedGraph::vertex_t) * occurrence->size);
            buf+=sizeof(UndirectedGraph::vertex_t) * occurrence->size;
        }
    }

    return buf;
}