//
// Created by steven on 10/3/17.
//

#include <iostream>
#include <set>
#include <algorithm>
#include "../common/graph/UndirectedGraph.h"
#include "../common/graph/SimpleGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/OptionsParser.h"

int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-decompose. Version: " << MOTIVO_VERSION_STRING << std::endl;

    OptionsParser op;
    OptionsParser::Option* help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option* path_opt = op.add_option(false, true, "path", '\0', "", "Use a path of ARG vertices");
    OptionsParser::Option* star_opt = op.add_option(false, true, "star", '\0', "", "Use a star of ARG vertices");
    OptionsParser::Option* root_opt = op.add_option(false, true, "root", '\0', "", "Only decompose the treelet rootet at vertex ARG (default: use all vertices as roots)");
    OptionsParser::Option* colored_opt =  op.add_option(false, false, "colored", '\0', "", "Decompose using a fixed coloring of the vertices");
    OptionsParser::Option* size_opt = op.add_option(false, true, "size", '\0', "", "Only print treelets with ARG vertices (default: print all treelets)");

    bool parse_ok = op.parse(argc, argv);
    if(!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-decompose [OPTION]..." << std::endl;
        std::cout << "  Decomposes a graph in list of edges format into its rooted treelets" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return parse_ok?EXIT_SUCCESS:EXIT_FAILURE;
    }

    if(!op.has_required_options())
    {
        std::cout << "Required options are missing" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
    	SimpleGraph g;
        unsigned int size=0;
        if(size_opt->is_found())
        {
            int s= std::stoi(size_opt->get_value());
            if(s<=0 || s>16)
                throw std::runtime_error("Invalid size");
            size = static_cast<unsigned int>(s);
        }

        if(path_opt->is_found() && star_opt->is_found())
            throw new std::runtime_error("Options 'path' and 'star' cannot be used at the same time");

        if(path_opt->is_found())
        {
            int s = std::stoi(path_opt->get_value());
            if(s<=0 || s>16)
                throw std::runtime_error("Invalid path size");
            g = SimpleGraph::path(static_cast<unsigned int>(s));
        }
        else if(star_opt->is_found())
        {
            int s = std::stoi(star_opt->get_value());
            if(s<=0 || s>16)
                throw std::runtime_error("Invalid star size");
            g = SimpleGraph::star(static_cast<unsigned int>(s));
        }
        else
            g = SimpleGraph::from_stdin();

        int root=-1;
        if(root_opt->is_found())
        {
            root = std::stoi(root_opt->get_value());
            if(root<0 || static_cast<unsigned int>(root)>= g.number_of_vertices())
                throw std::runtime_error("Invalid root");
        }

        SimpleGraph::treelet_set_t treelets;
        treelets.set_empty_key(Treelet::invalid_treelet);
        g.decompose(&treelets, root);

        Treelet::treelet_structure_t previous_structure = Treelet::invalid_structure;
        for(const Treelet& t : treelets)
        {
            if(!colored_opt->is_found() && t.get_structure()==previous_structure)
                continue;

            if(size!=0 && t.number_of_vertices()!=size)
                continue;

            std::cout << t.get_structure() << " " << (colored_opt->is_found() ? t.get_colors() : 0) << "\n";
            previous_structure = t.get_structure();
        }
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
