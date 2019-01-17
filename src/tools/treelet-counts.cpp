//
// Created by steven on 6/21/18.
//
#include <map>
#include <exception>
#include "../common/util.h"
#include "../common/treelets/TreeletTable.h"



int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-treelet-counts. Version: " << MOTIVO_VERSION_STRING << std::endl;

    if(argc!=2)
    {
        std::cerr << "Usage: motivo-treelet-counts table-filename" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        std::cout << "Loading table" << std::endl;

        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias> reader(argv[1]);
        TreeletTable table(&reader);

        std::map<Treelet, TreeletTable::treelet_count_t> counts;
        for (UndirectedGraph::vertex_t u = 0; u < table.number_of_vertices(); u++)
            for (TreeletTable::const_iterator it = table.begin(u); !it.is_over(); ++it)
                counts[it.treelet()] += it.count();

        for(const auto& it : counts)
            std::cout << it.first.get_structure() << " " << it.first.get_colors() << " " << uint128_to_string(it.second) << "\n";
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}