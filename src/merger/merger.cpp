//
// Created by steven on 12/10/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>

#include "../common/UndirectedGraph.h"
#include "../common/Treelet.h"
#include "../common/TreeletTable.h"
#include "boost/program_options.hpp"

namespace po = boost::program_options;

int main(const int argc, const char** argv)
{

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Print help and exit")
            ("o,output", po::value<std::string>(), "Output basename (required)")
            ("input", po::value<std::vector<std::string>>(), "Input count files");

    po::positional_options_description p;
    p.add("input", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
    po::notify(vm);


    if (vm.count("help"))
    {
        std::cout << "motivo-merge [OPTION]... FILE [FILE]..." << std::endl;
        std::cout << "  Builds treelet tables for use with motivo-sample" << std::endl << std::endl;
        std::cout << desc << std::endl;

        return EXIT_SUCCESS;
    }

    if(!vm.count("output"))
    {
        std::cout << "'output' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }

    auto& count_files = vm["input"].as<std::vector<std::string>>();
    if(count_files.size()==0)
    {
        std::cout << "No inputs specified" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string& base_output_filename = vm["output"].as<std::string>();

    std::string output_filename = base_output_filename + ".dat";
    std::ofstream out(  output_filename, std::ofstream::binary | std::ofstream::trunc);

    std::string offset_filename = base_output_filename + ".off";
    std::ofstream off(  offset_filename, std::ofstream::binary | std::ofstream::trunc);

    AliasMethodSampler *alias_sampler = nullptr;
    UndirectedGraph::vertex_t num_vertices = 0;

    uint64_t num_records=0;
    UndirectedGraph::vertex_t processed_vertices=0;
    for(unsigned int i=0; i<count_files.size(); i++)
    {
        const std::string& filename = count_files[i];
        std::ifstream f(filename, std::ifstream::binary);
        if(f.bad())
        {
            std::cout << "Unable to open file " << filename << std::endl;
            return EXIT_FAILURE;
        }

        UndirectedGraph::vertex_t nv, from, to;
        f.read(reinterpret_cast<char*>(&nv), sizeof(UndirectedGraph::vertex_t));
        f.read(reinterpret_cast<char*>(&from), sizeof(UndirectedGraph::vertex_t));
        f.read(reinterpret_cast<char*>(&to), sizeof(UndirectedGraph::vertex_t));

        if(i==0)
        {
            num_vertices = nv;
            alias_sampler = new AliasMethodSampler(num_vertices);
            uint64_t nv64=nv;
            off.write(reinterpret_cast<char*>(&nv64), sizeof(uint64_t));
        }
        else if(num_vertices!=nv)
        {
            std::cout << "Error while processing " << filename << ": wrong number of vertices" << std::endl;
            return EXIT_FAILURE;
        }

        if(from!=processed_vertices)
        {
            std::cout << "Error while processing " << filename << ": intervals are not consectuve" << std::endl;
            return EXIT_FAILURE;
        }

        TreeletTable::treelet_count_t total=0;
        while(true)
        {
            Treelet t;
            TreeletTable::treelet_count_t c;
            f.read(reinterpret_cast<char*>(&t), sizeof(Treelet));
            f.read(reinterpret_cast<char*>(&c), sizeof(TreeletTable::treelet_count_t));

            if(!f.good())
                break;

            if(t.is_valid())
                add_overflow(total, c, &total);
            else //New vertex
            {
                if(processed_vertices!=0)
                    alias_sampler->set(processed_vertices-1, total);

                off.write(reinterpret_cast<char*>(&num_records), sizeof(uint64_t));
                processed_vertices++;
                total = 0;
            }

            out.write(reinterpret_cast<char*>(&t), sizeof(Treelet));
            out.write(reinterpret_cast<char*>(&total), sizeof(TreeletTable::treelet_count_t));

            add_overflow(num_records, static_cast<uint64_t>(1), &num_records);
        }

        alias_sampler->set(processed_vertices-1, total);

        f.close();

        if(to!=processed_vertices-1)
        {
            std::cout << "Error while processing " << filename << ": number of vertices does not match number of records" << std::endl;
            return EXIT_FAILURE;
        }
    }

    off.write(reinterpret_cast<char*>(&num_records), sizeof(uint64_t));

    out.close();
    off.close();

    std::string root_sampler_filename = base_output_filename + ".rts";
    alias_sampler->build();
    alias_sampler->write(root_sampler_filename);

    delete alias_sampler;

    std::cout << "Processed " << processed_vertices << " vertices (" << num_records <<" records)" << std::endl;
    std::cout << "Output written to files: " << output_filename << ", " << offset_filename << ", and " << root_sampler_filename << std::endl;

    return EXIT_SUCCESS;
}