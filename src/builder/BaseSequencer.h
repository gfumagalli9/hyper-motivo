//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_BASESEQUENCER_H
#define MOTIVO_BASESEQUENCER_H


#include "../common/UndirectedGraph.h"

class BaseSequencer
{
public:
    struct sequence_batch_t
    {
        UndirectedGraph::vertex_t from;
        UndirectedGraph::vertex_t to;
    };

    virtual sequence_batch_t next_batch() = 0;

};


#endif //MOTIVO_BASESEQUENCER_H
