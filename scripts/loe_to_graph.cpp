#include<iostream>
#include <cassert>
#include<map>
#include<vector>
#include<set>


#ifdef NUMERIC
    typedef uint32_t key_type;
#else
    typedef std::string key_type;
#endif

const std::string whitespace = " \t\r";
std::vector< std::set<uint32_t> > edges;
uint32_t n=0;
uint64_t m=0;

uint32_t getID(std::map<const key_type, uint32_t>& map, const key_type& name)
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

#ifdef NUMERIC
        first=std::stoul(str.substr(a, b-a));
        second=std::stoul(str.substr(c, d-c));
#else
        first=str.substr(a, b-a);
        second=str.substr(c, d-c);
#endif
        return true;
}

void read()
{
        std::map<const key_type, uint32_t> vertices;
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
