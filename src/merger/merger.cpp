//
// Created by steven on 12/10/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <stdlib.h>
#include "../common/UndirectedGraph.h"
#include "../common/Treelet.h"
#include "../common/TreeletTable.h"
#include "../common/OptionsParser.h"

unsigned int bits_needed(uint128_t n)
{
    unsigned int needed = 1;
    while(n>=2)
    {
        n/=2;
        needed++;
    }

    return needed;
}

std::string to_string(uint128_t n)
{
    static const constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
    static const constexpr uint128_t ten_38 = ten_19*ten_19; //Maximum power of 10 representable with an uint128_t

    if(n==0)
        return "0";

    std::string s = "";
    bool significant_digit_found = false;
    for (uint128_t max_dec=ten_38; max_dec!=0; max_dec/=10)
    {
        unsigned int digit = static_cast<unsigned int>(n / max_dec);
        n = n%max_dec;
        assert(digit<=9);
        if(significant_digit_found || digit!=0)
        {
            significant_digit_found = true;
            s += static_cast<char>('0' + digit);
        }
    }

    return s;
}

struct vertex_info
{
    char* ptr;
    TreeletTable::treelet_count_t count;
};

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, vertex_info* info);

void merge(const std::vector<std::string>& count_filenames, const std::string& output_basename)
{
    const unsigned long no_files = count_filenames.size();
    UndirectedGraph::vertex_t num_vertices = 0;
    std::pair<char*, size_t>* cnt_map = new std::pair<char*, size_t>[no_files];
    FILE** count_files = new FILE*[no_files];
    vertex_info* info = nullptr;
    std::vector<bool> seen_vertices;
    for(unsigned int i=0; i<no_files; i++)
    {
        const std::string &filename = count_filenames[i];
        count_files[i] = fopen(filename.c_str(), "rb");

        if(count_files[i]==NULL)
            throw std::runtime_error("Unable to open file " + filename );

        UndirectedGraph::vertex_t nv;
        fread(&nv, sizeof(UndirectedGraph::vertex_t), 1, count_files[i]);

        if(i==0)
        {
            num_vertices = nv;
            info = new vertex_info[num_vertices];
            seen_vertices.resize(num_vertices);
        }
        else if(num_vertices != nv)
            throw std::runtime_error("Error while processing " + filename + ": wrong number of vertices");

        fseeko(count_files[i], 0L, SEEK_END);
        off_t size = ftello(count_files[i]);
        assert(size>=0);
        assert(static_cast<std::make_unsigned<off_t>::type>(size) <= std::numeric_limits<size_t>::max());
        cnt_map[i].second =  static_cast<size_t>(size);
        cnt_map[i].first = static_cast<char*>(motivo_mmap(cnt_map[i].second, PROT_READ, fileno(count_files[i])));

        if(cnt_map[i].first == MAP_FAILED)
            throw std::runtime_error("Error while processing " + filename + ": cannot mmap file");

        const char* end = cnt_map[i].first + cnt_map[i].second;
        char* ptr = cnt_map[i].first + sizeof(UndirectedGraph::vertex_t);
        while(ptr + sizeof(UndirectedGraph::vertex_t) + sizeof(TreeletTable::treelet_count_t) <= end)
        {
            UndirectedGraph::vertex_t vertex;
            memcpy(&vertex, ptr, sizeof(UndirectedGraph::vertex_t));
            ptr+=sizeof(UndirectedGraph::vertex_t);

            TreeletTable::treelet_count_t nocc;
            memcpy(&nocc, ptr, sizeof(TreeletTable::treelet_count_t));
            ptr += sizeof(TreeletTable::treelet_count_t);

            assert(vertex<num_vertices);
            if(seen_vertices[vertex])
                throw std::runtime_error("Error while processing " + filename + ": duplicate vertex");

            seen_vertices[vertex]=true;

            info[vertex].ptr = ptr;
            info[vertex].count = nocc;

            ptr += nocc * sizeof(TreeletTable::treelet_count_pair);
        }

        if(ptr!=end)
            throw std::runtime_error("Error while processing " + filename + ": abnormal file termination");

        std::cout << "Loaded offsets for file " << filename << " vertices" << std::endl;
    }

    std::cout << "Writing output" << std::endl;
    write_table(output_basename, num_vertices, info);

    delete[] info;

    for(unsigned int i=0; i<no_files; i++)
    {
        motivo_munmap(cnt_map[i].first, cnt_map[i].second);
        fclose(count_files[i]);
    }

    delete[] cnt_map;
    delete[] count_files;
}

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, vertex_info* info)
{
    std::string output_filename = output_basename + ".dat";
    std::ofstream out(output_filename, std::ofstream::binary | std::ofstream::trunc);

    std::string offset_filename = output_basename + ".off";
    std::ofstream off(offset_filename, std::ofstream::binary | std::ofstream::trunc);

    uint64_t nv64=num_vertices;
    off.write(reinterpret_cast<char*>(&nv64), sizeof(uint64_t));

    uint64_t num_records_total=0;
    TreeletTable::treelet_count_t num_occ_treelet = 0;
    uint128_t num_occ_total = 0;
    uint128_t num_occ_max = 0;
    bool num_occ_total_overflow = false;
    AliasMethodSampler<UndirectedGraph::vertex_t, TreeletTable::treelet_count_t> alias_sampler(num_vertices);

    for(UndirectedGraph::vertex_t u=0; u < num_vertices; u++)
    {
        off.write(reinterpret_cast<char*>(&num_records_total), sizeof(uint64_t));
        safe_add(num_records_total, info[u].count, &num_records_total);
        safe_add(num_records_total, 1, &num_records_total);

        TreeletTable::treelet_count_t total=0;
        TreeletTable::treelet_count_pair tcp;
        tcp.treelet = Treelet::invalid_treelet;
        tcp.count = 0;

        out.write(reinterpret_cast<char*>(&tcp), sizeof(TreeletTable::treelet_count_pair));

        for(;info[u].count!=0;info[u].count--)
        {
            memcpy(&tcp, info[u].ptr, sizeof(TreeletTable::treelet_count_pair));
            info[u].ptr += sizeof(TreeletTable::treelet_count_pair);

            if(num_occ_treelet < tcp.count)
                num_occ_treelet = tcp.count;

            safe_add(total, tcp.count, &total);
            tcp.count=total;
            out.write(reinterpret_cast<char*>(&tcp), sizeof(TreeletTable::treelet_count_pair));
        }

        if( add_overflow(num_occ_total, total, &num_occ_total) )
            num_occ_total_overflow = true;

        if(total>num_occ_max)
            num_occ_max=total;

        alias_sampler.set(u, total);
    }

    off.write(reinterpret_cast<char*>(&num_records_total), sizeof(uint64_t));
    off.close();
    out.close();

    std::cout << "Building root sampler alias table... ";
    std::string root_sampler_filename = output_basename + ".rts";
    alias_sampler.build();
    alias_sampler.write(root_sampler_filename);
    std::cout << "done" << std::endl;

    std::cout << "Processed " << num_vertices << " vertices (wrote " << num_records_total << " records)" << std::endl;
    std::cout << "Total number of treelet occurrences: ";
    if(num_occ_total_overflow)
        std::cout <<"Overflow!" << std::endl;
    else
        std::cout << to_string(num_occ_total) << " (" << bits_needed(num_occ_total) << " bits)" << std::endl;
    std::cout << "Maximum number of treelet occurrences rooted in a single vertex: " << to_string(num_occ_max) << " ("<< bits_needed(num_occ_max) << " bits)" << std::endl;
    std::cout << "Maximum number of occurrences of a single rooted treelet: " << to_string(num_occ_treelet) << " ("<< bits_needed(num_occ_treelet) << " bits)" << std::endl;
    std::cout << "Output written to files: " << output_filename << ", " << offset_filename << ", and " << root_sampler_filename << std::endl;
}


int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-merge. Version: " << MOTIVO_VERSION_STRING << std::endl;

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output basename (required)");

    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-merge [OPTION]... FILE [FILE]..." << std::endl;
        std::cout << "  Builds treelet tables for use with motivo-sample" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return parse_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if(!op.has_required_options())
    {
        std::cout << "Required options are missing" << std::endl;
        return EXIT_FAILURE;
    }

    const std::vector<std::string> &count_files = op.positional_arguments();
    if (count_files.size() == 0)
    {
        std::cout << "No inputs specified" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        merge(count_files, output_opt->get_value());
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
