//
// Created by steven on 8/15/17.
//

//
// Created by steven on 12/3/16.
//

#include <fstream>
#include <mpi.h>
#include "../../common/UndirectedGraph.h"
#include "../TreeletSampler.h"
#include "../sampler_opts.h"
#include "../sampler_impl.h"
#include "../../mpi/MotivoMPIContext.h"
#include "../../mpi/protocol.h"
#include "../../mpi/MPIRemoteCompressedRecordFile.h"

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-sample-slave. Version: " << MOTIVO_VERSION_STRING << std::endl;

    MPI_Init(const_cast<int*>(&argc), const_cast<char***>(&argv));

    MotivoMPIContext context;
    int sampler_rank = context.hello(protocol::PARTICIPANT_SLAVE_SAMPLER);
    unsigned int sampler_size = context.number_of_slave_samplers();
    std::cout << "I am sampler with rank " << sampler_rank << " out of " << sampler_size << " samplers" << "\n";
    std::cout << "I am process with rank " << context.world_rank() << " out of " << context.world_size() << " processes" << std::endl;

    if(context.number_of_master_samplers()!=1)
        MPI_Abort(MPI_COMM_WORLD, 2);

    int master_rank = context.master_samplers_ranks()[0];
    MPI_Status status;
    sampler_opts opts;
    MPI_Recv(&opts, sizeof(sampler_opts), MPI_BYTE, master_rank, protocol::MSG_SAMPLER_ARGS, MPI_COMM_WORLD, &status);

    try
    {
        std::ostream* output = &std::cout;
        if(strlen(opts.output_basename)!=0)
            output = new std::ofstream(std::string(opts.output_basename) + "." + std::to_string(sampler_rank) + ".samples", std::ofstream::binary | std::ofstream::trunc);

        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;


        TreeletTableCollection ttc;
        MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>** readers =
                new MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>*[opts.size-1];
        TreeletTable** tables = new TreeletTable*[opts.size-1];

        std::cerr << "Loading tables and root sampler" << std::endl;

        for(unsigned int i=0; i<opts.size; i++)
        {
            readers[i] = new MPIRemoteCompressedRecordFile<const TreeletTable::treelet_count_pair_maybe_alias,TreeletTable::may_alias>(MPI_COMM_WORLD, G.number_of_vertices(), i+1, &context);
            tables[i] = new TreeletTable(readers[i]);
            ttc.add(tables[i]);
        }
        tables[opts.size-1]->load_root_sampler(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".rts" );

        Random rng(opts.seed);
        std::cerr << "Using seed " << rng.get_seed() << std::endl;
        std::cerr << "Sampling..." << std::endl;

        MPI_Barrier(MPI_COMM_WORLD);

        sample(G, ttc, opts.size, opts.number_of_samples, opts.number_of_accepted_samples, *output, opts.text,
               opts.canonicize, opts.graphlets, opts.norejection, opts.footprints, opts.spanning_trees, opts.vertices, &rng);

        MPI_Send(nullptr, 0, MPI_BYTE, master_rank, protocol::MSG_SAMPLER_SLAVE_DONE, MPI_COMM_WORLD);

        if(strlen(opts.output_basename)!=0)
            delete output;

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