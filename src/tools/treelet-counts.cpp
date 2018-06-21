//
// Created by steven on 6/21/18.
//
#include <map>
#include <exception>
#include "../common/treelets/TreeletTable.h"

inline std::string to_string(uint128_t n) {
    static const constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
    static const constexpr uint128_t ten_38 = ten_19 * ten_19; //Maximum power of 10 representable with an uint128_t

    if (n == 0)
        return "0";

    std::string s = "";
    bool significant_digit_found = false;
    for (uint128_t max_dec = ten_38; max_dec != 0; max_dec /= 10) {
        unsigned int digit = static_cast<unsigned int>(n / max_dec);
        n = n % max_dec;
        assert(digit <= 9);
        if (significant_digit_found || digit != 0) {
            significant_digit_found = true;
            s += static_cast<char>('0' + digit);
        }
    }

    return s;
}

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
            std::cout << it.first.get_structure() << " " << it.first.get_colors() << " " << to_string(it.second) << "\n";
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}