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

#include <iostream>
#include <set>
#include <algorithm>
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/OptionsParser.h"

/**
 * A simple graph(let) that can hold at most 16 vertices
 */
class SimpleGraph
{
public:
    typedef std::set<Treelet::treelet_structure_t> treelet_structure_set_t;

private:
    unsigned int nverts=0;
    unsigned int degrees[16] = {0};
    unsigned int adj_lists[16][16] = {{0}};

    Treelet dfs(unsigned int u, unsigned int parent, bool *visited, treelet_structure_set_t &structures)
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

            child_treelets[nchild_treelets++] = dfs(v, u, visited, structures);
        }

        std::sort(child_treelets, child_treelets+nchild_treelets);

        Treelet t = Treelet::singleton(static_cast<uint8_t>(u));
        while(nchild_treelets!=0)
        {
            t=t.merge(child_treelets[--nchild_treelets]);
            assert(t.is_valid());
            structures.insert(t.get_structure());
        }

        return t;
    }

public:
    unsigned int number_of_vertices() { return nverts; };

    void decompose(treelet_structure_set_t &structures, int root=-1)
    {
        bool visited[16];
        if(root == -1)
        {
            for(UndirectedGraph::vertex_t u = 0; u < 16; u++)
            {
                memset(visited, 0, sizeof(bool) * 16);
                dfs(u, u, visited, structures);
            }
        }
        else
        {
            memset(visited, 0, sizeof(bool) * 16);
            dfs(0, 0, visited, structures);
        }
    }

    static SimpleGraph treelet_from_stdin()
    {
        SimpleGraph g;
        bool seen[16]={false};
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
            for(unsigned int j=0; j<=g.degrees[u]; j++)
                existing |= (g.adj_lists[u][j]==v);

            if(existing)
            {
                std::cerr << "Duplicate edge" << std::endl;
                continue;
            }

            if(!seen[u])
            {
                seen[u]=true;
                g.nverts++;
            }

            if(!seen[v])
            {
                seen[v]=true;
                g.nverts++;
            }

            g.adj_lists[u][g.degrees[u]++]=v;
            g.adj_lists[v][g.degrees[v]++]=u;
            nedges++;
        }

        for(unsigned int i=0; i<g.nverts; i++)
        {
            if(!seen[i])
                throw std::runtime_error("Vertex IDs are not contiguous");
        }

        if(nedges!=g.nverts-1)
            throw std::runtime_error("Graph is not a tree");

        return g;
    }

    static SimpleGraph path(unsigned int size)
    {
        SimpleGraph g;
        g.nverts=size;
        for(unsigned int i=1; i<size; i++)
        {
            g.adj_lists[i-1][g.degrees[i-1]++]=i;
            g.adj_lists[i][g.degrees[i]++]=i-1;
        }
        return g;
    }

    static SimpleGraph star(unsigned int size)
    {
        SimpleGraph g;
        g.nverts=size;
        for(unsigned int i=1; i<size; i++)
        {
            g.adj_lists[0][g.degrees[0]++]=i;
            g.adj_lists[i][g.degrees[i]++]=0;
        }
        return g;
    }
};

int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-decompose. Version: " << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

    OptionsParser op;
    OptionsParser::Option* help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option* path_opt = op.add_option(false, true, "path", '\0', "", "Use a path of ARG vertices");
    OptionsParser::Option* star_opt = op.add_option(false, true, "star", '\0', "", "Use a star of ARG vertices");
    OptionsParser::Option* root_opt = op.add_option(false, true, "root", '\0', "", "Only decompose the treelet rootet at vertex ARG (default: use all vertices as roots)");
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
            throw std::runtime_error("Options 'path' and 'star' cannot be used at the same time");

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
            g = SimpleGraph::treelet_from_stdin();

        int root=-1;
        if(root_opt->is_found())
        {
            root = std::stoi(root_opt->get_value());
            if(root<0 || static_cast<unsigned int>(root)>= g.number_of_vertices())
                throw std::runtime_error("Invalid root");
        }


        SimpleGraph::treelet_structure_set_t structures;
        g.decompose(structures, root);

        for(const Treelet::treelet_structure_t& s : structures)
            if(size==0 || Treelet::number_of_vertices(s)==size)
               std::cout << s << "\n";
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
