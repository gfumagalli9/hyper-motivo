//
// Created by steven on 8/5/17.
//

#ifndef MOTIVO_MPIREMOTECOMPRESSEDRECORDFILE_H
#define MOTIVO_MPIREMOTECOMPRESSEDRECORDFILE_H


#include <cstdint>
#include <mutex>
#include <mpi.h>
#include "../common/RecordCompressor.h"
#include "protocol.h"
#include "../common/CompressedRecordFile.h"
#include "MotivoMPIContext.h"
#include "../common/UndirectedGraph.h"

template<typename T, bool RAW> class MPIRemoteCompressedRecordFile : public BaseRecordSource<T>
{
private:
    const MPI_Comm commuicator;
    const uint64_t records_per_server;
    const unsigned int size;
    MotivoMPIContext* const context;


private:
    int get_server_rank(const uint64_t record_no) const
    {
        return context->tableserver_ranks()[record_no/records_per_server];
    }

public:
    MPIRemoteCompressedRecordFile(const MPI_Comm comm, const uint64_t number_of_records, const unsigned int size, MotivoMPIContext* ctx)
            : commuicator(comm), records_per_server(number_of_records/context->number_of_tableservers()), size(size), context(ctx)
    {}

    Record<T> get_record(const uint64_t record_no)
    {
        protocol::record_request_t request;
        request.record_no=record_no;
        request.size=size;

        ompi_status_public_t recv_status;

        int server = get_server_rank(record_no);
        uint64_t length;
        char* buffer = nullptr;

        context->lock();

        MPI_Send(&request, sizeof(protocol::record_request_t), MPI_BYTE, server, protocol::MSG_ROW_REQUEST, MPI_COMM_WORLD);
        MPI_Recv(&length, sizeof(uint64_t), MPI_BYTE, server, protocol::MSG_ROW_SIZE_RESPONSE, MPI_COMM_WORLD, &recv_status);

        if(length!=0)
        {
            buffer = new char[length];

            char* p=buffer;
            uint64_t left=length;
            while (left != 0)
            {
                int to_recv= (left <= static_cast<unsigned int>(std::numeric_limits<int>::max())) ? static_cast<int>(left) : std::numeric_limits<int>::max();
                MPI_Recv(p, to_recv, MPI_BYTE, server, protocol::MSG_ROW_DATA_RESPONSE, MPI_COMM_WORLD, &recv_status);
                left -= static_cast<unsigned int>(to_recv);
                p += to_recv;
            }
        }

        context->unlock();

        RecordCompressor::decompress_result_t<T> result = RecordCompressor::decompress<T, RAW>(buffer, length);
        if(result.allocated)
        {
            delete[] buffer;
            return Record<T>(result.ptr, result.len, result.ptr);
        }

        return Record<T>(result.ptr, result.len, buffer);

    }
};


#endif //MOTIVO_MPIREMOTECOMPRESSEDRECORDFILE_H
