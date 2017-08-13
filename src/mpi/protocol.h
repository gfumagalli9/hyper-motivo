//
// Created by steven on 8/6/17.
//

#ifndef MOTIVO_PROTOCOL_H
#define MOTIVO_PROTOCOL_H

#include <cstdint>
#include "config.h"

namespace protocol
{
    constexpr const int MSG_TABLESERVER_ARGS = 1;
    constexpr const int MSG_TABLESERVER_SHUTDOWN = 2;

    constexpr const int MSG_BUILDER_ARGS = 3;
    constexpr const int MSG_BUILDER_SLAVE_DONE = 4;

    constexpr const int MSG_ROW_REQUEST = 5;
    constexpr const int MSG_ROW_SIZE_RESPONSE = 6;
    constexpr const int MSG_ROW_DATA_RESPONSE = 7;
    constexpr const int MSG_NEXT_BATCH_REQUEST = 8;
    constexpr const int MSG_NEXT_BATCH_RESPONSE = 9;

    constexpr const int PARTICIPANT_BUILDER_MASTER = 0;
    constexpr const int PARTICIPANT_TABLESERVER = 1;
    constexpr const int PARTICIPANT_BUILDER_SLAVE = 2;
    constexpr const int PARTICIPANT_SAMPLER_MASTER = 3;
    constexpr const int PARTICIPANT_SAMPLER_SLAVE = 4;

    struct record_request_t
    {
        uint64_t record_no;
        unsigned int size;
    };

    struct tableserver_args_t
    {
        unsigned int size;
        char tables_basename[MOTIVO_ARG_MAX];
    };
}

#endif //MOTIVO_PROTOCOL_H
