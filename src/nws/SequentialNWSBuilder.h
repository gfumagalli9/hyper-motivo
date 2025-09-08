// SequentialNWSBuilder.h
#ifndef MOTIVO_SEQUENTIAL_NWS_BUILDER_H
#define MOTIVO_SEQUENTIAL_NWS_BUILDER_H

#include <cstdint>
#include <vector>
#include <ostream>
#include <utility>   
#include <unordered_set>
#include <unordered_map>
#include "../common/treelets/TreeletList.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/graph/Hypergraph.h"
#include "NWSBuilder.h"
#include "../common/types/EdgeSubtype.h"
#include "../common/types/EdgeSubtypeHash.h"

class SequentialNWSBuilder{
public:
    using Vertex  = Hypergraph::vertex_t;

    SequentialNWSBuilder(const Hypergraph* H, TreeletList* treelet_list, TreeletTable* treelet_table, std::ostream* output) noexcept;

    void build();

private:
    const Hypergraph*      H;
    const TreeletList*     treelet_list;
    const TreeletTable*    treelet_table;
    std::unordered_map<Treelet, std::vector<int>, Treelet::TreeletHash> nws_counts;
    std::queue<std::pair<Treelet, EdgeSubtype>>   work_queue;
    std::unordered_map<Treelet, std::unordered_set<EdgeSubtype>, Treelet::TreeletHash> visited;
    std::ostream*          output;
    NWSBuilder             builder;
};

#endif // MOTIVO_SEQUENTIAL_NWS_BUILDER_H