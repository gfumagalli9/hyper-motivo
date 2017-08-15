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

    tableservers = new int[size];
    master_builders = new int[size];
    slave_builders = new int[size];
    master_samplers = new int[size];
    slave_samplers = new int[size];
    unknown = new int[size];
}

MotivoMPIContext::~MotivoMPIContext()
{
    delete[] tableservers;
    delete[] master_builders;
    delete[] slave_builders;
    delete[] master_samplers;
    delete[] slave_samplers;
    delete[] unknown;
}

int MotivoMPIContext::hello(int type)
{
    nmasterbulders=0;
    ntableservers=0;
    nslavebuilders=0;
    nunknown=0;

    int* participants = new int[size];
    MPI_Allgather( &type, 1, MPI_INT, participants, 1, MPI_INT, MPI_COMM_WORLD);

    int type_rank=0;
    for(int i=0; i<size; i++)
    {
        if(participants[i]==type && i<=rank)
            type_rank++;

        if(participants[i]==protocol::PARTICIPANT_TABLESERVER)
            tableservers[ntableservers++]=i;
        else if(participants[i]==protocol::PARTICIPANT_MASTER_BUILDER)
            master_builders[nmasterbulders++]=i;
        else if(participants[i]==protocol::PARTICIPANT_SLAVE_BUILDER)
            slave_builders[nslavebuilders++]=i;
        else if(participants[i]==protocol::PARTICIPANT_MASTER_SAMPLER)
            master_samplers[nmastersamplers++]=i;
        else if(participants[i]==protocol::PARTICIPANT_SLAVE_SAMPLER)
            slave_samplers[nslavesamplers++]=i;
        else
            unknown[nunknown++]=i;
    }

    delete[] participants;

    return type_rank;
}