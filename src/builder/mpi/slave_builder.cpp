//
// Created by steven on 8/13/17.
//

#include <iostream>
#include <mpi.h>
#include "../builder.h"
#include "../../mpi/protocol.h"
#include "../../mpi/MotivoMPIContext.h"
#include "../../common/treelets/TreeletTableCollection.h"
#include "../../mpi/MPISequencer.h"
#include "../TreeletTableBuilder.h"
#include "../../mpi/MPIRemoteCompressedRecordFile.h"

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-build-slave. Version: " << MOTIVO_VERSION_STRING << std::endl;

    int thread_support_provided;
    MPI_Init_thread(const_cast<int*>(&argc), const_cast<char***>(&argv), MPI_THREAD_SERIALIZED, &thread_support_provided);

    if(thread_support_provided!=MPI_THREAD_SERIALIZED)
        MPI_Abort(MPI_COMM_WORLD, 255);

    MotivoMPIContext context;
    int builder_rank = context.hello(protocol::PARTICIPANT_SLAVE_BUILDER);
    unsigned int builder_size = context.number_of_slave_builders();
    std::cout << "I am builder with rank " << builder_rank << " out of " << builder_size
              << " builders (process with rank " << context.world_rank() << " out of " << context.world_size() << " processes)" << std::endl;

    if(context.number_of_master_builders()!=1)
        MPI_Abort(MPI_COMM_WORLD, 2);

    int master_rank = context.master_bulders_ranks()[0];
    MPI_Status status;
    builder_opts opts;
    MPI_Recv(&opts, sizeof(builder_opts), MPI_BYTE, master_rank, protocol::MSG_BUILDER_ARGS, MPI_COMM_WORLD, &status);

    try
    {
        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cout << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        assert(opts.size!=1);

        TreeletTableCollection ttc;
        MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>** readers =
            new MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>*[opts.size-1];
        TreeletTable** tables = new TreeletTable*[opts.size-1];

        std::cout << "Loading tables for smaller sizes" << std::endl;

        for(unsigned int i=0; i<opts.size-1; i++)
        {
            readers[i] = new MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>(MPI_COMM_WORLD, G.number_of_vertices(), i+1, &context);
            tables[i] = new TreeletTable(readers[i]);
            ttc.add(tables[i]);
        }

        const std::string filename = std::string(opts.output_basename) + "." + std::to_string(opts.size) + "." + std::to_string(builder_rank) + ".cnt";
        std::ofstream out(filename , std::ofstream::binary | std::ofstream::trunc);
        if(out.bad())
            throw std::runtime_error("Could not open output file for writing");

        std::cout << "Computing counts of treelets of size " << opts.size << " for vertices " << opts.from_vertex << "--"
                  << opts.to_vertex << " using " << opts.threads << " worker thread(s)" << std::endl;

        MPISequencer sequencer(MPI_COMM_WORLD, master_rank, &context);

        TreeletTableBuilder builder(&G, nullptr, opts.size, &ttc, &out, &sequencer, opts.store0, opts.threads);

        MPI_Barrier(MPI_COMM_WORLD);

        builder.build();

        MPI_Send(nullptr, 0, MPI_BYTE, master_rank, protocol::MSG_BUILDER_SLAVE_DONE, MPI_COMM_WORLD);

        out.close();
        std::cout << "Output written to " << filename << std::endl;

        for(unsigned int i=0; i<opts.size-1; i++)
        {
            delete tables[i];
            delete readers[i];
        }

        delete[] tables;
        delete[] readers;
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 254);
        return EXIT_FAILURE;
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
