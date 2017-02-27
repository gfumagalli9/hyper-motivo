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
#include "../common/OptionsParser.h"

void merge(const std::vector<std::string>& count_files, const std::string& output_basename)
{
    std::string output_filename = output_basename + ".dat";
    std::ofstream out(  output_filename, std::ofstream::binary | std::ofstream::trunc);

    std::string offset_filename = output_basename + ".off";
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
            throw std::runtime_error("Unable to open file " + filename );

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
            throw std::runtime_error("Error while processing " + filename + ": wrong number of vertices");

        if(from!=processed_vertices)
            throw std::runtime_error("Error while processing " + filename + ": intervals are not consectuve");

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
            throw std::runtime_error("Error while processing " + filename + ": number of vertices does not match number of records");
    }

    off.write(reinterpret_cast<char*>(&num_records), sizeof(uint64_t));

    out.close();
    off.close();

    std::string root_sampler_filename = output_basename + ".rts";
    alias_sampler->build();
    alias_sampler->write(root_sampler_filename);

    delete alias_sampler;

    std::cout << "Processed " << processed_vertices << " vertices (" << num_records <<" records)" << std::endl;
    std::cout << "Output written to files: " << output_filename << ", " << offset_filename << ", and " << root_sampler_filename << std::endl;
}

int main(const int argc, const char** argv)
{
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