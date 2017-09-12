//
// Created by steven on 12/10/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include "../common/graph/UndirectedGraph.h"
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletTable.h"
#include "../common/OptionsParser.h"
#include "../common/io/CompressedRecordFile.h"

unsigned int bits_needed(uint128_t n)
{
    unsigned int needed = 1;
    for(n>>=1; n!=0; n>>=1)
        needed++;

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
    uint64_t count=0;
};

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, vertex_info* info, double compression_threshold);

void merge(const std::vector<std::string>& count_filenames, const std::string& output_basename, double compression_threshold)
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
        cnt_map[i].first = static_cast<char*>(motivo_mmap_populate(cnt_map[i].second, PROT_READ, fileno(count_files[i])));

        if(cnt_map[i].first == MAP_FAILED)
            throw std::runtime_error("Error while processing " + filename + ": cannot mmap file");

        const char* end = cnt_map[i].first + cnt_map[i].second;
        char* ptr = cnt_map[i].first + sizeof(UndirectedGraph::vertex_t);
        while(ptr + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) <= end)
        {
            UndirectedGraph::vertex_t vertex;
            memcpy(&vertex, ptr, sizeof(UndirectedGraph::vertex_t));
            ptr+=sizeof(UndirectedGraph::vertex_t);

            uint64_t number_of_occurrences;
            memcpy(&number_of_occurrences, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);

            assert(vertex<num_vertices);
            if(seen_vertices[vertex])
                throw std::runtime_error("Error while processing " + filename + ": duplicate vertex " + std::to_string(vertex));

            seen_vertices[vertex]=true;

            info[vertex].ptr = ptr;
            info[vertex].count = number_of_occurrences;

            ptr += number_of_occurrences * sizeof(TreeletTable::treelet_count_pair);
        }

        if(ptr!=end)
            throw std::runtime_error("Error while processing " + filename + ": abnormal file termination");

        std::cout << "Loaded offsets for file " << filename << " vertices" << std::endl;
    }

    std::cout << "Writing output" << std::endl;
    write_table(output_basename, num_vertices, info, compression_threshold);

    delete[] info;

    for(unsigned int i=0; i<no_files; i++)
    {
        motivo_munmap(cnt_map[i].first, cnt_map[i].second);
        fclose(count_files[i]);
    }

    delete[] cnt_map;
    delete[] count_files;
}

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, vertex_info* info, double compression_threshold)
{
    uint64_t num_treelet_count_pairs=0;
    TreeletTable::treelet_count_t num_occ_treelet = 0;
    uint128_t num_occ_total = 0;
    uint128_t num_occ_max = 0;
    bool num_occ_total_overflow = false;

    std::string output_filename = output_basename + ".dtz";
    CompressedRecordFileWriter writer(output_filename, num_vertices);

    AliasMethodSampler<UndirectedGraph::vertex_t, TreeletTable::treelet_count_t> alias_sampler(num_vertices);

    for(UndirectedGraph::vertex_t u=0; u < num_vertices; u++)
    {
        num_treelet_count_pairs+=info[u].count;
        TreeletTable::treelet_count_pair *to_write = new TreeletTable::treelet_count_pair[info[u].count + 1];
        TreeletTable::treelet_count_pair *p = to_write;
        p->treelet = Treelet::invalid_treelet;
        p->count = 0;
        for (TreeletTable::treelet_count_t i = 0; i < info[u].count; i++)
        {
            p++;
            memcpy(p, info[u].ptr, sizeof(TreeletTable::treelet_count_pair));

            if(num_occ_treelet < p->count)
                num_occ_treelet = p->count;

            p->count += (p-1)->count;
            info[u].ptr += sizeof(TreeletTable::treelet_count_pair);
        }
        writer.write_record(reinterpret_cast<char*>(to_write), (info[u].count+1) * sizeof(TreeletTable::treelet_count_pair), compression_threshold);

        if( add_overflow(num_occ_total, p->count, &num_occ_total) )
            num_occ_total_overflow = true;

        if(p->count > num_occ_max)
            num_occ_max= p->count;

        alias_sampler.set(u, p->count);
        delete[] to_write;
    }

    writer.close();

    std::cout << "Compressed size: " << writer.get_compressed_size() << " Original size: " << writer.get_uncompressed_size()
              << " Ratio: " << static_cast<double>(writer.get_compressed_size())/static_cast<double>(writer.get_uncompressed_size()) << std::endl;

    std::cout << "Building root sampler alias table... ";
    std::string root_sampler_filename = output_basename + ".rts";
    alias_sampler.build();
    alias_sampler.write(root_sampler_filename);
    std::cout << "done" << std::endl;

    std::cout << "Processed " << num_vertices << " vertices (wrote " << num_treelet_count_pairs << " counts)" << std::endl;
    std::cout << "Total number of treelet occurrences: ";
    if(num_occ_total_overflow)
        std::cout <<"Overflow!" << std::endl;
    else
        std::cout << to_string(num_occ_total) << " (" << bits_needed(num_occ_total) << " bits)" << std::endl;
    std::cout << "Maximum number of occurrences rooted in a single vertex: " << to_string(num_occ_max) << " ("<< bits_needed(num_occ_max) << " bits)" << std::endl;
    std::cout << "Maximum number of occurrences of a single rooted treelet: " << to_string(num_occ_treelet) << " ("<< bits_needed(num_occ_treelet) << " bits)" << std::endl;
    std::cout << "Output written to files: " << output_filename << ", and " << root_sampler_filename << std::endl;
}


int main(const int argc, const char** argv)
{
    std::cout << "This is motivo-merge. Version: " << MOTIVO_VERSION_STRING << std::endl;

    OptionsParser op;
    OptionsParser::Option *help_opt = op.add_option(false, false, "help", '\0', "", "Print help and exit");
    OptionsParser::Option *compress_opt = op.add_option(false, true, "compress-threshold", '\0', "1", "Compress records if the compressed size is less than ARG times the uncompressed size (default: 1, 0 disables compression)");
    OptionsParser::Option *output_opt = op.add_option(true, true, "output", 'o', "", "Output basename (required)");


    bool parse_ok = op.parse(argc, argv);
    if (!parse_ok || help_opt->is_found())
    {
        std::cout << "motivo-merge [OPTION]... FILE [FILE]..." << std::endl;
        std::cout << "  Builds treelet tables for use with motivo-sample" << std::endl << std::endl;
        std::cout << op.help() << std::endl;

        return parse_ok?EXIT_SUCCESS:EXIT_FAILURE;
    }

    if(!op.has_required_options())
    {
        std::cout << "Required options are missing" << std::endl;
        return EXIT_FAILURE;
    }

    double compress_threshold;
    try
    {
        compress_threshold=std::stod(compress_opt->get_value());
    }
    catch(std::exception &e)
    {
        std::cout << "Invalid compress-thresold" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Compress threshold is: " << compress_threshold << std::endl;


    const std::vector<std::string> &count_files = op.positional_arguments();
    if (count_files.size() == 0)
    {
        std::cout << "No inputs specified" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        merge(count_files, output_opt->get_value(), compress_threshold);
    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
