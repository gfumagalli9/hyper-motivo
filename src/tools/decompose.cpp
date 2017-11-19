//
// Created by steven on 10/3/17.
//

#include <iostream>
#include <set>
#include <algorithm>
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/OptionsParser.h"

unsigned int nverts=0;
unsigned int  degrees[16] = {0};
unsigned int  adj_lists[16][16] = {0};

Treelet dfs(unsigned int u, unsigned int parent, bool *visited, std::set<Treelet> *treelets)
{
    visited[u]=true;

    int nchild_treelets=0;
    Treelet child_treelets[15];

    for(unsigned int i=0; i<degrees[u]; i++)
    {
        unsigned int v = adj_lists[u][i];
        if(parent==v)
            continue;

        if(visited[v])
            throw std::runtime_error("Graph is not a tree");

        child_treelets[nchild_treelets++] = dfs(v, u, visited, treelets);
    }

    std::sort(child_treelets, child_treelets+nchild_treelets);

    Treelet t = Treelet::singleton(static_cast<uint8_t>(u));
    while(nchild_treelets!=0)
    {
        t=t.merge(child_treelets[--nchild_treelets]);
        assert(t.is_valid());
        treelets->insert(t);
    }

    return t;
}

void decompose(std::set<Treelet> *treelets, int root)
{
    bool visited[16];

    if(root==-1)
    {
        for(UndirectedGraph::vertex_t u=0; u<16; u++)
        {
            memset(visited, 0, sizeof(bool)*16);
            dfs(u, u, visited, treelets);
        }
    }
    else
    {
        memset(visited, 0, sizeof(bool)*16);
        dfs(0, 0, visited, treelets);
    }
}

void read_graph()
{
    bool seen[16]={0};
    unsigned int nedges=0;

    unsigned int  u,v;
    while(std::cin >> u >> v)
    {
        if(u>=16 || v>=16 || u==v)
        {
            std::cerr << "Invalid edge" << std::endl;
            continue;
        }

        bool existing=false;
        for(unsigned int j=0; j<=degrees[u]; j++)
            existing |= (adj_lists[u][j]==v);

        if(existing)
        {
            std::cerr << "Duplicate edge" << std::endl;
            continue;
        }

        if(!seen[u])
        {
            seen[u]=true;
            nverts++;
        }

        if(!seen[v])
        {
            seen[v]=true;
            nverts++;
        }

        adj_lists[u][degrees[u]++]=v;
        adj_lists[v][degrees[v]++]=u;
        nedges++;
    }

    for(unsigned int i=0; i<nverts; i++)
    {
        if(!seen[i])
            throw std::runtime_error("Vertex IDs are not contiguous");
    }

    if(nedges!=nverts-1)
        throw std::runtime_error("Graph is not a tree");
}

void path(unsigned int size)
{
    nverts=size;
    for(unsigned int i=1; i<size; i++)
    {
        adj_lists[i-1][degrees[i-1]++]=i;
        adj_lists[i][degrees[i]++]=i-1;
    }
}

void star(unsigned int size)
{
    nverts=size;
    for(unsigned int i=1; i<size; i++)
    {
        adj_lists[0][degrees[0]++]=i;
        adj_lists[i][degrees[i]++]=0;
    }
}

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
            path(static_cast<unsigned int>(s));
        }
        else if(star_opt->is_found())
        {
            int s = std::stoi(star_opt->get_value());
            if(s<=0 || s>16)
                throw std::runtime_error("Invalid star size");
            star(static_cast<unsigned int>(s));
        }
        else
            read_graph();

        int root=-1;
        if(root_opt->is_found())
        {
            root = std::stoi(root_opt->get_value());
            if(root<0 || static_cast<unsigned int>(root)>=nverts)
                throw std::runtime_error("Invalid root");
        }

        std::set<Treelet> treelets;
        decompose(&treelets, root);

        Treelet::treelet_structure_t previous_structure = Treelet::invalid_structure;
        for(std::set<Treelet>::iterator it=treelets.begin(); it!=treelets.end(); it++)
        {
            if(!colored_opt->is_found() && it->get_structure()==previous_structure)
                continue;

            if(size!=0 && it->number_of_vertices()!=size)
                continue;

            std::cout << it->get_structure() << " " << (colored_opt->is_found() ? it->get_colors() : 0) << "\n";
            previous_structure = it->get_structure();
        }
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}