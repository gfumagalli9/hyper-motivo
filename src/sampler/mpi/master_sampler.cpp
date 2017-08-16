//
// Created by steven on 8/15/17.
//

#include <mpi.h>
#include "../sampler_opts.h"
#include "../../mpi/MotivoMPIContext.h"
#include "../../mpi/protocol.h"

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build-master. Version: " << MOTIVO_VERSION_STRING << std::endl;
    MPI_Init(const_cast<int*>(&argc), const_cast<char***>(&argv));

    sampler_opts opts;
    try
    {
        if(!parse_sampler_args(argc, argv, "motivo-sample-master", &opts))
        {
            MPI_Abort(MPI_COMM_WORLD, 0);
            return EXIT_SUCCESS;
        }
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return EXIT_FAILURE;
    }


    MotivoMPIContext context;
    std::cout << "MPI Version: " << context.mpi_version() << std::endl;
    std::cout << "Master sampler process with rank " << context.world_rank() << " out of " << context.world_size() << " processes" << std::endl;

    context.hello(protocol::PARTICIPANT_MASTER_SAMPLER);

    if(context.number_of_master_samplers()!=1)
        MPI_Abort(MPI_COMM_WORLD, 2);

    if(context.number_of_tableservers()==0)
        MPI_Abort(MPI_COMM_WORLD, 3);

    if(context.number_of_slave_samplers()==0)
        MPI_Abort(MPI_COMM_WORLD, 4);

    if(context.number_of_master_builders()!=0 || context.number_of_slave_builders()!=0 || context.number_of_unknown()!=0)
        MPI_Abort(MPI_COMM_WORLD, 5);


    if(strlen(opts.seed)==0)
    {
        Random rng(opts.seed);
        strcpy(opts.seed, rng.get_seed().c_str());
    }

    const int* samplers = context.slave_samplers_ranks();
    for(unsigned int i=0; i<context.number_of_slave_samplers(); i++)
    {
        sampler_opts slave_opts = opts;
        strcpy(slave_opts.seed,  (std::string(opts.seed) + "@" + std::to_string(i)).c_str());
        MPI_Send(&slave_opts, sizeof(sampler_opts), MPI_BYTE, samplers[i], protocol::MSG_SAMPLER_ARGS, MPI_COMM_WORLD);
    }

    protocol::tableserver_args_t tableserver_args;
    tableserver_args.size=opts.size;
    strcpy(tableserver_args.tables_basename, opts.tables_basename);
    const int* tableservers = context.tableservers_ranks();
    for(unsigned int i=0; i<context.number_of_tableservers(); i++)
        MPI_Send(&tableserver_args, sizeof(protocol::tableserver_args_t), MPI_BYTE, tableservers[i], protocol::MSG_TABLESERVER_ARGS, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    unsigned int samplers_done = 0;
    while(samplers_done<context.number_of_slave_samplers())
    {
        ompi_status_public_t recv_status;
        MPI_Recv(nullptr, 0, MPI_BYTE, MPI_ANY_SOURCE, protocol::MSG_SAMPLER_SLAVE_DONE, MPI_COMM_WORLD, &recv_status);

        if(recv_status.MPI_ERROR)
            MPI_Abort(MPI_COMM_WORLD, 6);

        samplers_done++;
    }

    for(unsigned int i=0; i<context.number_of_tableservers(); i++)
        MPI_Send(nullptr, 0, MPI_BYTE, tableservers[i], protocol::MSG_TABLESERVER_SHUTDOWN, MPI_COMM_WORLD);

    MPI_Finalize();

    return EXIT_SUCCESS;
}