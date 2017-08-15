//
// Created by steven on 8/7/17.
//

#ifndef MOTIVO_MOTIVOMPICONTEXT_H
#define MOTIVO_MOTIVOMPICONTEXT_H

#include <mutex>

class MotivoMPIContext
{
private:
    int rank;
    int size;

    unsigned int ntableservers = 0;
    unsigned int nmasterbulders = 0;
    unsigned int nslavebuilders = 0;
    unsigned int nmastersamplers = 0;
    unsigned int nslavesamplers = 0;
    unsigned int nunknown = 0;

    int *tableservers;
    int *master_builders;
    int *slave_builders;
    int *master_samplers;
    int *slave_samplers;
    int *unknown;

    std::mutex mutex;

    std::string version;

public:
    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    const std::string mpi_version() { return version; }

    int world_rank() const { return rank; }
    int world_size() const { return size; }

    unsigned int number_of_tableservers() const { return ntableservers; }
    unsigned int number_of_master_builders() const { return nmasterbulders; }
    unsigned int number_of_slave_builders() const { return nslavebuilders; }
    unsigned int number_of_master_samplers() const { return nmastersamplers; }
    unsigned int number_of_slave_samplers() const { return nslavesamplers; }
    unsigned int number_of_unknown() const { return nunknown; }

    const int* tableservers_ranks() const { return tableservers; }
    const int* master_bulders_ranks() const { return master_builders; }
    const int* slave_builders_ranks() const { return slave_builders; }
    const int* master_samplers_ranks() const { return master_samplers; }
    const int* slave_samplers_ranks() const { return slave_samplers; }
    const int* unknown_ranks() const { return unknown; }

    MotivoMPIContext();
    ~MotivoMPIContext();
    int hello(int type);
};

#endif //MOTIVO_MOTIVOMPICONTEXT_H
