//
// Created by steven on 8/15/17.
//

#include "sampler_impl.h"
#include "Occurrence.h"
#include "TreeletSampler.h"


void sample (const UndirectedGraph &G, const TreeletTableCollection &ttc, const unsigned int size, const uint64_t num_samples,
             const uint64_t num_accepted, std::ostream& out, const  bool text, const bool canonicize, const  bool graphlets,
             const bool no_rejection, const bool footprints, const bool spanning_trees_no, const  bool vertices, Random* rng)
{
    TreeletSampler sampler(&G, &ttc, rng);
    UndirectedGraph::vertex_t sampled_vertices[16] = {0};

    //FIXME: Cache spanning trees count and/or footprints?

    std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();

    uint64_t sampled=0;
    uint64_t accepted=0;

    Occurrence occurrence;
    uint64_t spanning_trees = 1;
    while(sampled<num_samples && accepted<num_accepted)
    {
        sampled++;
        UndirectedGraph::vertex_t root = sampler.sample_root(size);
        assert(root<G.number_of_vertices());
        Treelet t = sampler.sample_treelet(size, root);

        if(vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
        {
#ifndef NDEBUG
            bool success =
#endif
                    sampler.sample_rooted_occurrence(t, root, sampled_vertices);
            assert(success);
        }

        if(graphlets)
        {
            new (&occurrence) Occurrence(size, &G, sampled_vertices);

            if(!no_rejection || spanning_trees_no)
                spanning_trees = occurrence.number_of_spanning_trees();

            if(!no_rejection && rng->random_uint<uint64_t>(0, spanning_trees-1)!=0)
                continue; //Rejection
        }
        else
            new (&occurrence) Occurrence(t, sampled_vertices);

        if(canonicize)
            occurrence.canonicize();

        if(text)
        {
            if(footprints)
                out << occurrence.text_footprint() << ";";

            if(spanning_trees_no)
                out << spanning_trees << ";";

            if(vertices)
            {
                const UndirectedGraph::vertex_t* verts = occurrence.vertices();
                for(unsigned int i=0; i<size; i++)
                    out << verts[i] << ((i==size-1)?";":" ");
            }

            out << "\n";
        }
        else
        {
            if(footprints)
                out.write(occurrence.binary_footprint(), Occurrence::binary_footprint_bytes);

            if(spanning_trees_no)
                out.write(reinterpret_cast<const char*>(&spanning_trees), sizeof(uint64_t));

            if(vertices)
                out.write(reinterpret_cast<const char*>(occurrence.vertices()), static_cast<std::streamsize>(sizeof(UndirectedGraph::vertex_t)*occurrence.size));
        }

        accepted++;
    }

    std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;

    std::cerr << "Sampling time: " << delta_t.count() << " s\n";
    std::cerr << "Sampled treelets: " << sampled << " (" << static_cast<double>(sampled)/delta_t.count() << " occ/s)" << "\n";
    std::cerr << "Accepted treelets/graphlets: " << accepted<< " (" << static_cast<double>(accepted)/delta_t.count() << " occ/s)" << "\n";
    std::cerr << "Rejected treelets/graphlets: " << sampled - accepted << std::endl;
}