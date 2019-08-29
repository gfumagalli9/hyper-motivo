// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <map>
#include <exception>
#include "../common/util.h"
#include "../common/treelets/TreeletTable.h"

int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-treelet-counts. Version: " << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

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