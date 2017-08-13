//
// Created by steven on 8/5/17.
//

#include <cstdio>
#include <mpi.h>
#include "../common/CompressedRecordFile.h"
#include "protocol.h"
#include "MotivoMPIContext.h"


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
    MPI_Init(&argc, &argv);

    MotivoMPIContext context;
    int server_rank = context.hello(protocol::PARTICIPANT_TABLESERVER);
    assert(server_rank>=0);
    unsigned int server_size = context.number_of_tableservers();
    assert(server_size>=1);
    std::cout << "I am server with rank " << server_rank << " out of " << server_size << " servers" << "\n";
    std::cout << "I am process with rank " << context.world_rank() << " out of " << context.world_size() << " processes" << std::endl;


    protocol::tableserver_args_t tableserver_args;
    int master_rank = context.master_ranks()[0];
    MPI_Status status;
    MPI_Recv(&tableserver_args, sizeof(protocol::tableserver_args_t), MPI_BYTE, master_rank,protocol::MSG_TABLESERVER_ARGS, MPI_COMM_WORLD, &status);

    CompressedRecordFileReader<const char, true>* readers = new CompressedRecordFileReader<const char, true>[tableserver_args.size];
    for(unsigned int i=0; i<tableserver_args.size; i++)
    {
        readers[i].open(std::string(tableserver_args.tables_basename)+"."+std::to_string(i+1)+".dtz");
        uint64_t chunk = readers[i].number_of_records()/server_size;

        uint64_t from = chunk * static_cast<unsigned int>(server_rank);
        uint64_t to = ( static_cast<unsigned int>(server_rank) == server_size-1)?(readers[i].number_of_records()-1):((chunk+1)*static_cast<unsigned int>(server_rank) - 1);

        readers[i].prefault(from, to);
    }


    MPI_Barrier(MPI_COMM_WORLD);


    loop(readers);

    MPI_Finalize();

    delete[] readers;

    return EXIT_SUCCESS;
}
