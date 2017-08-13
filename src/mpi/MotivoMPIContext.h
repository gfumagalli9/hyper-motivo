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

    unsigned int nmasters = 0;
    unsigned int ntableservers = 0;
    unsigned int nbuilders = 0;
    unsigned int nunknown = 0;

    int *masters;
    int *tableservers;
    int *builders;
    int *unknown;

    std::mutex mutex;

    std::string version;

public:
    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    const std::string mpi_version() { return version; }

    int world_rank() const { return rank; }
    int world_size() const { return size; }

    unsigned int number_of_masters() const { return nmasters; }
    unsigned int number_of_tableservers() const { return ntableservers; }
    unsigned int number_of_builders() const { return nbuilders; }
    unsigned int number_of_unknown() const { return nunknown; }

    const int* master_ranks() const { return masters; }
    const int* tableserver_ranks() const { return tableservers; }
    const int* builder_ranks() const { return builders; }
    const int* unknown_ranks() const { return unknown; }

    MotivoMPIContext();
    ~MotivoMPIContext();
    int hello(int type);
};

#endif //MOTIVO_MOTIVOMPICONTEXT_H
