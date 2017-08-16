//
// Created by steven on 8/15/17.
//

#ifndef MOTIVO_SAMPLER_IMPL_H
#define MOTIVO_SAMPLER_IMPL_H


#include "../common/Random.h"
#include "../common/TreeletTableCollection.h"

void sample [[gnu::hot]] (const UndirectedGraph &G, const TreeletTableCollection &ttc, const unsigned int size, const uint64_t num_samples,
                          const uint64_t num_accepted, std::ostream& out, const  bool text, const bool canonicize, const  bool graphlets,
                          const bool no_rejection, const bool footprints, const bool spanning_trees_no, const  bool vertices, Random* rng);

#endif //MOTIVO_SAMPLER_IMPL_H
