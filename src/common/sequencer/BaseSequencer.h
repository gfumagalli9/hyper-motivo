//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_BASESEQUENCER_H
#define MOTIVO_BASESEQUENCER_H


template <typename T> class BaseSequencer
{
public:
    struct sequence_batch_t
    {
        T from;
        T to;
    };

    virtual sequence_batch_t next_batch() = 0;
    virtual ~BaseSequencer() {};

};


#endif //MOTIVO_BASESEQUENCER_H
