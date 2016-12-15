//
// Created by steven on 12/10/16.
//

#include <iostream>
#include <fstream>
#include <vector>

#include "../common/UndirectedGraph.h"
#include "../common/Treelet.h"
#include "../common/TreeletTable.h"
#include "../../libs/cxxopts.hpp"

int main(int argc, char** argv)
{
    cxxopts::Options options("motivo-merge", "Builds treelet tables for use with motivo-sample", "[OPTIONS...] FILE...");

    options.add_options("")
            ("help", "Print help and exit")
            ("o,output", "Output basename (required)", cxxopts::value<std::string>(), "BASENAME")
            ("positional", "Positional arguments", cxxopts::value<std::vector<std::string>>());

    options.parse_positional(std::vector<std::string>{"positional"});
    options.parse(argc, argv);

    if(options.count("help"))
    {
        std::cout << options.help() << std::endl;
        std::cout << "FILE... is a list of count tables to merge" << std::endl;
        return EXIT_SUCCESS;
    }

    if(!options["output"].count())
    {
        std::cout << "'output' parameter is required" << std::endl;
        return EXIT_FAILURE;
    }

    auto& count_files = options["positional"].as<std::vector<std::string>>();
    if(count_files.size()==0)
    {
        std::cout << "No inputs specified" << std::endl;
        return EXIT_FAILURE;
    }

    std::string output_filename = options["output"].as<std::string>() + ".dat";
    std::ofstream out(  output_filename, std::ofstream::binary | std::ofstream::trunc);

    std::string offset_filename = options["output"].as<std::string>() + ".off";
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

    std::string root_sampler_filename = options["output"].as<std::string>() + ".rts";
    alias_sampler->build();
    alias_sampler->write(root_sampler_filename);

    delete alias_sampler;

    std::cout << "Processed " << processed_vertices << " vertices (" << num_records <<" records)" << std::endl;
    std::cout << "Output written to files: " << output_filename << ", " << offset_filename << ", and " << root_sampler_filename << std::endl;

    return EXIT_SUCCESS;
}