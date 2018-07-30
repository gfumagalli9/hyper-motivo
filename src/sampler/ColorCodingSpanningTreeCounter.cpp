#include "ColorCodingSpanningTreeCounter.h"

ColorCodingSpanningTreeCounter::ColorCodingSpanningTreeCounter(const Occurrence *occurrence, TreeletSelector *selector)
        : occurrence(occurrence), selector(selector), size(occurrence->get_size())
{
    if(!occurrence->is_valid())
        throw new std::runtime_error("Invalid occurrence");
}

ColorCodingSpanningTreeCounter::~ColorCodingSpanningTreeCounter()
{
    if(tables==nullptr)
        return;

    for(unsigned int  i = 1; i<=size; i++)
        delete[] tables[i-1];

    delete[] tables;
}


void ColorCodingSpanningTreeCounter::count()
{
    if(tables!=nullptr)
        return;

    //tables[s-1][i] contains the table of size s for vertex i
    tables = new table_t*[size];

    //Fill table of size 1
    tables[0] = new table_t[size];
    for(uint8_t u = 0; u < size; u++)
    {
        COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[0][u]);
        tables[0][u][Treelet::singleton(u)] = 1;
    }

    //Fill tables of sizes 2,...,size
    for(unsigned int i = 2; i<=size; i++)
    {
        tables[i-1] = new table_t[size];
        do_build(i);
    }
}



void ColorCodingSpanningTreeCounter::do_build(const unsigned int current_size)
{
    for(unsigned int u = 0; u < occurrence->get_size(); u++)
    {
        COLORCODINGSPANNINGTREECOUNTER_INIT_HASHMAP(tables[current_size-1][u]);

        for(unsigned int v = 0; v < u; v++)
        {
            if(occurrence->has_edge(u,v)) //u > v must hold
            {
                combine(u, v, current_size);
                combine(v, u, current_size);
            }
        }
    }

    //Normalization
    //Alternatively we could also keep a vector of 'TreeletCountPair's for each size and vertex
    //(as opposed to one hashtable per size per vertex)
    //In this case a single hashtable would be used to count the occurrences of treelets for a single vertex
    //Then its contents are copied to a vector (and normalized) and sorted.
    //This requires more computational effort but allows to break early in combine()
    for(unsigned int u = 0; u < size; u++)
    {
        for (auto &entry : tables[current_size-1][u])
        {
            assert(entry.second % entry.first.normalization_factor() == 0);
            entry.second /= entry.first.normalization_factor();
        }
    }
}




void ColorCodingSpanningTreeCounter::combine(const unsigned int u, const unsigned int v, const unsigned int current_size)
{
    for(unsigned int size1 = 1; size1 < current_size; size1++)
    {
        for (const auto &entry1 : tables[size1-1][u])
        {
            assert(entry1.first.is_valid());
            assert(entry1.second != 0);

            for (const auto &entry2 : tables[current_size-size1-1][v])
            {
                assert(entry1.first.is_valid());
                assert(entry2.second != 0);

                Treelet merged = entry1.first.merge(entry2.first);
                if (!merged.is_valid() || (selector && !selector->is_included(merged)))
                    continue; //We could break early in case of invalid merge if we use a sorted vector (like in TreeletTableBuilder)

                tables[current_size-1][u][merged] += entry1.second * entry2.second;
            }
        }
    }
}

uint64_t ColorCodingSpanningTreeCounter::number_of_rooted_spanning_trees()
{
    uint64_t spanning_trees = 0;
    for(unsigned int u = 0; u < occurrence->get_size(); u++)
        for (auto &entry : tables[size-1][u])
            spanning_trees += entry.second;

    return spanning_trees;
}

uint64_t ColorCodingSpanningTreeCounter::number_of_spanning_trees_rooted_at(unsigned int root)
{
    uint64_t spanning_trees = 0;
    for (auto &entry : tables[size-1][root])
            spanning_trees += entry.second;

    return spanning_trees;
}



