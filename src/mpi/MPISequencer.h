//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_MPISEQUENCER_H
#define MOTIVO_MPISEQUENCER_H

#include <mpi.h>
#include "../builder/BaseSequencer.h"
#include "protocol.h"
#include "MotivoMPIContext.h"

class MPISequencer : public BaseSequencer
{
private:
    const MPI_Comm commuicator;
    const int sequencer_server;
    MotivoMPIContext* context;

public:
    MPISequencer(const MPI_Comm comm, const int server, MotivoMPIContext* context) : commuicator(comm), sequencer_server(server), context(context) {}

    BaseSequencer::sequence_batch_t next_batch()
    {
        BaseSequencer::sequence_batch_t batch;
        ompi_status_public_t status;

        context->lock();
        MPI_Send(nullptr, 0, MPI_BYTE, sequencer_server, protocol::MSG_NEXT_BATCH_REQUEST, commuicator);
        MPI_Recv(&batch, sizeof(BaseSequencer::sequence_batch_t), MPI_BYTE, sequencer_server, protocol::MSG_NEXT_BATCH_RESPONSE, commuicator, &status);
        context->unlock();

        return batch;
    }
};


#endif //MOTIVO_MPISEQUENCER_H
