#include<iostream>
#include <cassert>
#include<map>
#include<vector>
#include<set>

const std::string whitespace = " \t\r";
std::map<const std::string, uint32_t> vertices;
std::vector< std::set<uint32_t> > edges;
uint32_t n=0;
uint64_t m=0;

uint32_t getID(const std::string& name)
{
        auto it = vertices.find(name);
        if(it!=vertices.end())
                return it->second;
        
        vertices[name] = n;
        edges.emplace_back();
        std::cerr << name << std::endl;
        return n++;
}

bool split(const std::string& str, std::string& first, std::string& second)
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
        std::string line, first, second;
        while(std::getline(std::cin, line))
        {
                if(!split(line, first, second))
                        continue;
                
                uint32_t u = getID(first);
                uint32_t v = getID(second);
                
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

        edges.shrink_to_fit();
}

void write()
{
        std::cout << n << " " << m << std::endl;
        for(uint32_t u=0; u<n; u++)
        {
                std::cout << edges[u].size();
                for(auto it : edges[u])
                {
                        std::cout << " " << it;
                        if(it>u)
                                edges[it].insert(u);
                }
                std::cout << std::endl;
                edges[u] = std::set<uint32_t>(); //Make sure memory is released
        }
}

int main()
{
        read();
        write();
        return EXIT_SUCCESS;       
}
