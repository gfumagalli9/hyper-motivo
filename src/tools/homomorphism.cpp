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
#include <config.h>
#include "../common/treelets/Treelet.h"
#include "../common/OptionsParser.h"

class SJTPermutationGenerator
{
private:
    const unsigned int n;
    unsigned int* elements;
    signed char* direction;

public:
    explicit SJTPermutationGenerator(const unsigned int n) : n(n)
    {
        if(n==0)
            throw std::runtime_error("n must be positive");

        elements = new unsigned int[n];
        direction = new signed char[n];

        elements[0] = 0;
        direction[0] = 0;
        for(unsigned int i=1; i<n; i++)
        {
            elements[i]=i;
            direction[i]=-1;
        }
    }

    ~SJTPermutationGenerator()
    {
        delete[] elements;
        delete[] direction;
    }

    const unsigned int* permutation() const { return elements; }

    bool next()
    {
        unsigned int j=0;
        unsigned int x=0;
        bool found=false;
        for(unsigned int i=0; i<n; i++) // finds the largest element with a nonzero direction
        {
            if(direction[i] != 0 && elements[i] >= x)
            {
                found=true;
                j=i;
                x=elements[i];
            }
        }

        if(!found)
            return false;

        unsigned int k = (direction[j]>0)?(j+1):(j-1);

        {unsigned int t=elements[j]; elements[j]=elements[k]; elements[k]=t;}
        {signed char t=direction[j]; direction[j]=direction[k]; direction[k]=t;}

        //If this causes the chosen element to reach the first or last position within the permutation,
        //or if the next element in the same direction is larger than the chosen element, the direction of the chosen element is set to zero
        if(k==0 || k==n-1 || elements[(direction[k]>0)?(k+1):(k-1)] > x)
            direction[k]=0;

        for(unsigned int i=0; i<k; i++)
            if(elements[i]>x)
                direction[i]=1;

        for(unsigned int i=k+1; i<n; i++)
            if(elements[i]>x)
                direction[i]=-1;

        return true;
    }
};


unsigned int nverts=0;
unsigned int parents[16] = {0};
bool adj_matrix[16][16] = {{false}};

inline bool is_homomorfism(const unsigned int* perm)
{
    for(unsigned int u=1; u<nverts; u++)
    {
        if(!adj_matrix[perm[u]][perm[parents[u]]])
            return false;
    }

    return true;
}

uint64_t compute()
{
    uint64_t count=0;
    SJTPermutationGenerator P(nverts);
    do
    {
        count+=is_homomorfism(P.permutation());
    } while(P.next());

    return count;
}

void parse_treelet_structure(const std::string& treelet_structure_str)
{
    static_assert(std::numeric_limits<unsigned long long>::max() >= std::numeric_limits<Treelet::treelet_structure_t>::max(), "treelet structure does not fit in unsigned long long" );
    uint64_t treelet_structure = std::stoull(treelet_structure_str);

    if(treelet_structure>=std::numeric_limits<Treelet::treelet_structure_t>::max())
        throw std::runtime_error("Invalid treelet structure");

    Treelet t(static_cast<Treelet::treelet_structure_t>(treelet_structure));
    nverts = t.number_of_vertices();

    unsigned int current = 0;
    unsigned int n=0;
    for(Treelet::treelet_structure_t structure = t.get_structure(); structure; structure<<=1)
    {
        if(structure & Treelet::treelet_structure_highest_bit)
        {
            n++;
            parents[n]=current;
            current=n;
        }
        else
            current=parents[current];
    }

    assert(n==nverts-1);
}

void parse_occurrence_footprint(const std::string& footprint)
{
    if (footprint.length() != 30) // FIXME Occurrence::text_footprint_bytes)
        throw std::runtime_error("Occurrence footprint has wrong length");

    uint8_t edges[15] = {0}; ////FIXME: [Occurrence::binary_footprint_bytes] = {0};

    const char *c = footprint.c_str();
    for (unsigned int i = 0; i < 15; i++, c+=2) //FIXME: i<Occurrence::binary_footprint_bytes; i++)
    {
        if (*c < 'A' || *c >= 'A' + 16 || *(c + 1) < 'A' || *(c + 1) >= 'A' + 16)
            throw std::runtime_error("Occurrence footprint is invalid");

        edges[i] = static_cast<uint8_t>(((*c - 'A') << 4) | (*(c + 1) - 'A'));
    }

    int pos = 0;
    unsigned int max_vertex=0;
    for (unsigned int i = 1; i < 16; i++)
    {
        for (unsigned int j = 0; j < i; j++)
        {
            adj_matrix[i][j] = adj_matrix[j][i] = static_cast<bool>(edges[pos / 8] & (0b10000000 >> (pos % 8)));
            pos++;

            if(adj_matrix[i][j])
                max_vertex=i;
        }
    }

    if(max_vertex!=nverts-1)
        throw std::runtime_error("Treelet size differs from occurrence size");
}

int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-homomorphism. Version: " << MOTIVO_VERSION_STRING << std::endl;

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *treelet_opt = op.add_option(true, true, "treelet", '\0', "", "Treelet structure");
    OptionsParser::Option *occurrence_opt = op.add_option(true, true, "occurrence", '\0', "", "Occurrence footprint");


    bool parse_ok = op.parse(argc, argv);
    if(!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-homomorphism [OPTION]..." << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;
        std::cout << "  Counts the number of induced homomorphisms from a treelet to an occurrence" << std::endl << std::endl;
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
        parse_treelet_structure(treelet_opt->get_value());
        parse_occurrence_footprint(occurrence_opt->get_value());
        std::cout << compute() << std::endl;

    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

