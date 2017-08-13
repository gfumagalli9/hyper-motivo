//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_STATICSEQUENCER_H
#define MOTIVO_STATICSEQUENCER_H

#include <atomic>
#include "BaseSequencer.h"

class StaticSequencer : public BaseSequencer
{
public:
    typedef void (*progress_callback_t)(UndirectedGraph::vertex_t);

private:
    std::atomic<UndirectedGraph::vertex_t> next;
    const UndirectedGraph::vertex_t end;
    const UndirectedGraph::vertex_t batch_size;
    progress_callback_t progress_callback;
    UndirectedGraph::vertex_t progress_interval;

public:
    StaticSequencer(UndirectedGraph::vertex_t start, UndirectedGraph::vertex_t end, UndirectedGraph::vertex_t batch_size)
            : next(start), end(end), batch_size(batch_size), progress_callback(nullptr) {}

    BaseSequencer::sequence_batch_t next_batch()
    {
        sequence_batch_t batch;
        batch.from = next.fetch_add(batch_size);
        batch.to = batch.from + batch_size-1;

        if (batch.to>end)
            batch.to=end;

        if(progress_callback!=nullptr && (batch.from/progress_interval)<(batch.to/progress_interval))
            (*progress_callback)(batch.from);

        return batch;
    }

    void set_progress_callback(progress_callback_t callback, UndirectedGraph::vertex_t interval)
    {
        progress_callback = callback;
        progress_interval = interval;
    }
};


#endif //MOTIVO_STATICSEQUENCER_H
