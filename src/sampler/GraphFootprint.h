//
// Created by steven on 12/3/16.
//

#ifndef MOTIVO_GRAPHFOOTPRINT_H
#define MOTIVO_GRAPHFOOTPRINT_H


#include <cstdint>
#include "../UndirectedGraph.h"

class GraphFootprint
{

public:
    struct footprint
    {
        uint8_t data[16] = {0};

        std::string to_string();
    };

    footprint get_footprint(const UndirectedGraph* graph, const UndirectedGraph::vertex_t* subgraph, unsigned int size);

};


#endif //MOTIVO_GRAPHFOOTPRINT_H
