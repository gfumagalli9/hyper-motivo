//
// Created by steven on 8/5/17.
//

#include <cstdlib>
#include <cstdio>
#include <mpi.h>
#include "../../common/CompressedRecordFile.h"
#include "../../common/UndirectedGraph.h"
#include "../../mpi/protocol.h"
#include "../../common/OptionsParser.h"
#include "../StaticSequencer.h"
#include "../../mpi/MotivoMPIContext.h"
#include "../builder.h"

void loop(StaticSequencer &sequencer, const MotivoMPIContext* context)
{

    unsigned int builders = context->number_of_slave_builders();
    unsigned int builders_done = 0;
    while(builders_done<builders)
    {
        ompi_status_public_t recv_status;
        MPI_Recv(nullptr, 0, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_status);

        if(recv_status.MPI_TAG == protocol::MSG_BUILDER_SLAVE_DONE)
            builders_done++;
        else if(recv_status.MPI_TAG == protocol::MSG_NEXT_BATCH_REQUEST)
        {
            BaseSequencer::sequence_batch_t batch = sequencer.next_batch();
            MPI_Send(&batch, sizeof(BaseSequencer::sequence_batch_t), MPI_BYTE, recv_status.MPI_SOURCE, protocol::MSG_NEXT_BATCH_RESPONSE, MPI_COMM_WORLD);
        }
        else
            MPI_Abort(MPI_COMM_WORLD, 6);
    }
}

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build-master. Version: " << MOTIVO_VERSION_STRING << std::endl;
    MPI_Init(const_cast<int*>(&argc), const_cast<char***>(&argv));

    static builder_opts opts;
    try
    {
        if(!parse_builder_args(argc, argv, "motivo-build-master", &opts))
        {
            MPI_Abort(MPI_COMM_WORLD, 0);
            return EXIT_SUCCESS;
        }

        if(opts.size==1)
            throw std::runtime_error("Size of 1 is not supported by MPI builders");
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return EXIT_FAILURE;
    }

    MotivoMPIContext context;
    std::cout << "MPI Version: " << context.mpi_version() << std::endl;
    std::cout << "Master builder process with rank " << context.world_rank() << " out of " << context.world_size() << " processes" << std::endl;

    context.hello(protocol::PARTICIPANT_MASTER_BUILDER);

    if(context.number_of_master_builders()!=1)
        MPI_Abort(MPI_COMM_WORLD, 2);

    if(context.number_of_tableservers()==0)
        MPI_Abort(MPI_COMM_WORLD, 3);

    if(context.number_of_slave_builders()==0)
        MPI_Abort(MPI_COMM_WORLD, 4);

    if(context.number_of_master_samplers()!=0 || context.number_of_slave_samplers()!=0 || context.number_of_unknown()!=0)
        MPI_Abort(MPI_COMM_WORLD, 5);

    const int* builders = context.slave_builders_ranks();
    for(unsigned int i=0; i<context.number_of_slave_builders(); i++)
    {
        builder_opts slave_opts = opts;
        strcpy(slave_opts.seed,  (std::string(opts.seed) + "@" + std::to_string(i)).c_str());
        MPI_Send(&slave_opts, sizeof(builder_opts), MPI_BYTE, builders[i], protocol::MSG_BUILDER_ARGS, MPI_COMM_WORLD);
    }

    protocol::tableserver_args_t tableserver_args;
    tableserver_args.size=opts.size-1;
    strcpy(tableserver_args.tables_basename, opts.tables_basename);

    const int* tableservers = context.tableservers_ranks();
    for(unsigned int i=0; i<context.number_of_tableservers(); i++)
        MPI_Send(&tableserver_args, sizeof(protocol::tableserver_args_t), MPI_BYTE, tableservers[i], protocol::MSG_TABLESERVER_ARGS, MPI_COMM_WORLD);

    StaticSequencer sequencer(opts.from_vertex, opts.to_vertex, opts.batch_size);
    if(opts.progress > 0)
    {
        std::cout << "Will print a progress report every " << opts.progress << " processed vertices" << std::endl;
        sequencer.set_progress_callback( [](UndirectedGraph::vertex_t next) -> void { report_progress(next, opts.from_vertex, opts.to_vertex); }, opts.progress);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    loop(sequencer, &context);

    for(unsigned int i=0; i<context.number_of_tableservers(); i++)
        MPI_Send(nullptr, 0, MPI_BYTE, tableservers[i], protocol::MSG_TABLESERVER_SHUTDOWN, MPI_COMM_WORLD);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
