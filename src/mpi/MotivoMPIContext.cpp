//
// Created by steven on 8/7/17.
//

#include "protocol.h"
#include "MotivoMPIContext.h"
#include <mpi.h>
#include <string>

MotivoMPIContext::MotivoMPIContext()
{
    int len;
    char buf[MPI_MAX_LIBRARY_VERSION_STRING];
    MPI_Get_library_version(buf, &len);
    version = std::string(buf);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    masters = new int[size];
    tableservers = new int[size];
    builders = new int[size];
    unknown = new int[size];
}

MotivoMPIContext::~MotivoMPIContext()
{
    delete[] masters;
    delete[] tableservers;
    delete[] builders;
    delete[] unknown;
}

int MotivoMPIContext::hello(int type)
{
    nmasters=0;
    ntableservers=0;
    nbuilders=0;
    nunknown=0;

    int* participants = new int[size];
    MPI_Allgather( &type, 1, MPI_INT, participants, 1, MPI_INT, MPI_COMM_WORLD);

    int type_rank=0;
    for(int i=0; i<size; i++)
    {
        if(participants[i]==type && i<=rank)
            type_rank++;

        if(participants[i]==protocol::PARTICIPANT_BUILDER_MASTER)
            masters[nmasters++]=i;
        else if(participants[i]==protocol::PARTICIPANT_TABLESERVER)
            tableservers[ntableservers++]=i;
        else if(participants[i]==protocol::PARTICIPANT_BUILDER_SLAVE)
            builders[nbuilders++]=i;
        else
            unknown[nunknown++]=i;
    }

    delete[] participants;

    return type_rank;
}