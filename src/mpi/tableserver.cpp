//
// Created by steven on 8/5/17.
//

#include <cstdio>
#include <mpi.h>
#include "../common/CompressedRecordFile.h"
#include "protocol.h"
#include "MotivoMPIContext.h"


MotivoMPIContext context;
int server_rank;

void loop(CompressedRecordFileReader<const char, true>* const readers)
{
    while(true)
    {
        MPI_Status recv_status;

        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_status);
        if(recv_status.MPI_TAG == protocol::MSG_TABLESERVER_SHUTDOWN)
            break;

        protocol::record_request_t request;
        MPI_Recv(&request, sizeof(protocol::record_request_t), MPI_BYTE, MPI_ANY_SOURCE, protocol::MSG_ROW_REQUEST, MPI_COMM_WORLD, &recv_status);

#ifndef NDEBUG
        uint64_t chunk = 1 + readers[request.size-1].number_of_records()/context.number_of_tableservers();
        uint64_t from = chunk * static_cast<unsigned int>(server_rank);
        uint64_t to = chunk*static_cast<unsigned int>(server_rank+1) - 1;

        if(request.record_no < from || request.record_no>=to)
        {
            std::cerr << "Warning: request for record " << request.record_no << " for table of size " << request.size
                      << " from rank " << recv_status.MPI_SOURCE
                      << " that does not belong to this tableserver" << std::endl;
        }
#endif

        Record<const char> record = readers[request.size-1].get_raw(request.record_no);
        uint64_t left = record.length();
        MPI_Send(&left, sizeof(uint64_t), MPI_BYTE, recv_status.MPI_SOURCE, protocol::MSG_ROW_SIZE_RESPONSE, MPI_COMM_WORLD);

        const char* p=record.begin();
        while(left!=0)
        {
            int to_send = (left<= static_cast<unsigned int>(std::numeric_limits<int>::max())) ? static_cast<int>(left):std::numeric_limits<int>::max();
            MPI_Send(p, to_send, MPI_BYTE, recv_status.MPI_SOURCE, protocol::MSG_ROW_DATA_RESPONSE, MPI_COMM_WORLD);
            left-= static_cast<unsigned int>(to_send);
            p+=to_send;
        }

        record.free();
    }
}

int main(int argc, char* argv[])
{
    std::cout << "This is motivo-tableserver. Version: " << MOTIVO_VERSION_STRING << std::endl;

    MPI_Init(&argc, &argv);

    context.hello(protocol::PARTICIPANT_TABLESERVER);
    assert(server_rank>=0);
    assert(context.number_of_tableservers()>=1);
    std::cout << "I am table server with rank " << server_rank << " out of " << context.number_of_tableservers() << " table servers" << "\n";
    std::cout << "I am process with rank " << context.world_rank() << " out of " << context.world_size() << " processes" << std::endl;

    if( context.number_of_master_builders() + context.number_of_master_samplers() != 1 )
        MPI_Abort(MPI_COMM_WORLD, 2);

    protocol::tableserver_args_t tableserver_args;
    int master_rank = (context.number_of_master_builders()!=0)?context.master_bulders_ranks()[0]:context.master_samplers_ranks()[0];

    MPI_Status status;
    MPI_Recv(&tableserver_args, sizeof(protocol::tableserver_args_t), MPI_BYTE, master_rank, protocol::MSG_TABLESERVER_ARGS, MPI_COMM_WORLD, &status);

    CompressedRecordFileReader<const char, true>* readers = new CompressedRecordFileReader<const char, true>[tableserver_args.size];
    for(unsigned int i=0; i<tableserver_args.size; i++)
    {
        readers[i].open(std::string(tableserver_args.tables_basename)+"."+std::to_string(i+1)+".dtz");
        uint64_t chunk = 1 + readers[i].number_of_records()/context.number_of_tableservers();

        uint64_t from = chunk * static_cast<unsigned int>(server_rank);
        uint64_t to = chunk*static_cast<unsigned int>(server_rank+1) - 1;
        to = (to < readers[i].number_of_records())?to:(readers[i].number_of_records()-1);

        std::cout << "Prefaulting nodes " << from << " -- " << to << " for tables of size " << (i+1) << std::endl;
        readers[i].prefault(from, to);
    }


    MPI_Barrier(MPI_COMM_WORLD);


    loop(readers);

    MPI_Finalize();

    delete[] readers;

    return EXIT_SUCCESS;
}
