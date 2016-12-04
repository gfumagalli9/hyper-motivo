//
// Created by steven on 12/3/16.
//

#include <iostream>
#include <fstream>
#include "../UndirectedGraph.h"

int main(int argc, char** argv)
{
    if(argc!=3)
    {
        std::cout << "Usage: " << argv[0] << " input-graph output-basename" << std::endl;
        return EXIT_FAILURE;
    }

    UndirectedGraph::vertex_t num_verts;
    uint32_t num_edges;

    std::ifstream stream(argv[1]);

    std::ofstream offsets( std::string(argv[2]) + ".gof", std::ofstream::binary | std::ofstream::trunc);
    std::ofstream edges( std::string(argv[2]) + ".ged", std::ofstream::binary | std::ofstream::trunc);

    stream >> num_verts >> num_edges;
    offsets.write(reinterpret_cast<const char*>(&num_verts), sizeof(UndirectedGraph::vertex_t));
    offsets.write(reinterpret_cast<const char*>(&num_edges), sizeof(uint32_t));

    UndirectedGraph::vertex_t processed_edges = 0;
    for(UndirectedGraph::vertex_t u=0; u < num_verts; u++)
    {
        offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

        UndirectedGraph::vertex_t degree;
        stream >> degree;
        processed_edges += degree;

        UndirectedGraph::vertex_t v;
        for(UndirectedGraph::vertex_t i=0; i < degree; i++)
        {
            stream >> v;
            edges.write(reinterpret_cast<const char*>(&v), sizeof(UndirectedGraph::vertex_t));
        }
    }

    offsets.write(reinterpret_cast<const char*>(&processed_edges), sizeof(UndirectedGraph::vertex_t));

    edges.close();
    offsets.close();

    return EXIT_SUCCESS;
}
