#include<iostream>
#include <cassert>
#include<map>
#include<vector>
#include<set>

#include <sparsehash/sparse_hash_set>
#include <sparsehash/sparse_hash_map>

typedef std::string key_type;
typedef google::sparse_hash_map<key_type, uint32_t> table_t;

const std::string whitespace = " \t\r";
std::vector< google::sparse_hash_set<uint32_t> > edges;
uint32_t n=0;
uint64_t m=0;

uint32_t getID(table_t& map, const key_type& name)
{
        auto it = map.find(name);
        if(it!=map.end())
                return it->second;

        map[name] = n;
        edges.emplace_back();
        std::cerr << name << std::endl;
        return n++;
}

bool split(const std::string& str, key_type& first, key_type& second)
{
        size_t a = str.find_first_not_of(whitespace);
        assert(a!=std::string::npos);
        
        if(str[a]=='#')
                return false;
        
        size_t b = str.find_first_of(whitespace, a);
        assert(b!=std::string::npos);
        size_t c = str.find_first_not_of(whitespace, b);
        assert(c!=std::string::npos);
        size_t d = str.find_first_of(whitespace, c);
        if(d==std::string::npos)
                d=str.size();

        first=str.substr(a, b-a);
        second=str.substr(c, d-c);
        return true;
}

void read()
{
        table_t vertices;
        std::string line;
        key_type first, second;
        while(std::getline(std::cin, line))
        {
                if(!split(line, first, second))
                        continue;
                
                uint32_t u = getID(vertices, first);
                uint32_t v = getID(vertices, second);
                
                if(u==v)
                        continue;
                
                if(u>v)
                {
                        uint32_t t=u;
                        u=v;
                        v=t;
                }
                
                if(edges[u].insert(v).second)
                        m++;
        }
}

void write()
{
        std::cout << n << " " << m << std::endl;
        for(uint32_t u=0; u<n; u++)
        {
            uint32_t d = edges[u].size();
            std::cout << d;
            if(d)
            {
                uint32_t* neighbors = new uint32_t[d];
                std::copy(edges[u].begin(), edges[u].end(), neighbors);
                edges[u].clear();
                edges[u].resize(0);
                std::sort(neighbors, neighbors+d);
                for(uint32_t i=0; i<d; i++)
                {
                    std::cout << " " << neighbors[i];
                    if(neighbors[i]>u)
                        edges[neighbors[i]].insert(u);
                }
                delete[] neighbors;
            }
            std::cout << std::endl;
        }
}

int main()
{
        read();
        write();
        return EXIT_SUCCESS;       
}
