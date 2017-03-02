//
// Created by steven on 12/10/16.
//

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include "../common/UndirectedGraph.h"
#include "../common/Treelet.h"
#include "../common/TreeletTable.h"
#include "../common/OptionsParser.h"

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, const std::pair<std::ifstream *, std::streampos> *vertexpos);

void merge(const std::vector<std::string>& count_files, const std::string& output_basename)
{

    UndirectedGraph::vertex_t num_vertices = 0;
    UndirectedGraph::vertex_t processed_vertices = 0;
    std::pair<std::ifstream*, std::streampos> *vertexpos = nullptr;
    std::vector<std::ifstream*> streams;
    std::vector<bool> *seen_vertices = nullptr;
    for(unsigned int i=0; i<count_files.size(); i++)
    {
        const std::string& filename = count_files[i];
        std::ifstream* f = new std::ifstream(filename, std::ifstream::binary);
        if(f->bad())
            throw std::runtime_error("Unable to open file " + filename );

        streams.push_back(f);

        UndirectedGraph::vertex_t totalnv;
        f->read(reinterpret_cast<char*>(&totalnv), sizeof(UndirectedGraph::vertex_t));
        if(i==0)
        {
            num_vertices = totalnv;
            vertexpos = new std::pair<std::ifstream*, std::streampos>[num_vertices];
            seen_vertices = new std::vector<bool>(num_vertices);
        }
        else if(num_vertices!=totalnv)
            throw std::runtime_error("Error while processing " + filename + ": wrong number of vertices");


        UndirectedGraph::vertex_t file_vertices = 0;
        TreeletTable::treelet_count_t file_records = 0;
        while(true)
        {
            UndirectedGraph::vertex_t vertex;
            TreeletTable::treelet_count_t nrecords;

            f->read(reinterpret_cast<char*>(&vertex), sizeof(UndirectedGraph::vertex_t));
            if(!f->good())
                break;

            if((*seen_vertices)[vertex])
                throw std::runtime_error("Duplicate vertex");

            (*seen_vertices)[vertex]=true;

            file_vertices++;
            vertexpos[vertex] = std::make_pair(f, f->tellg());

            f->read(reinterpret_cast<char*>(&nrecords), sizeof(TreeletTable::treelet_count_t));
            file_records+=nrecords;
            f->ignore(nrecords * sizeof(TreeletTable::treelet_count_pair) );
        }

        f->clear();

        processed_vertices+=file_vertices;
        std::cout << "File " << filename << " contains counts for " << file_vertices << " vertices (" << file_records << " records)" << std::endl;
    }

    delete seen_vertices;

    if(processed_vertices!=num_vertices)
        throw std::runtime_error("Number of graph vertices does not match total number vertices in input files");

    write_table(output_basename, num_vertices, vertexpos);

    delete[] vertexpos;
    for(unsigned int i=0; i<streams.size(); i++)
    {
        streams[i]->close();
        delete streams[i];
    }
}

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices, const std::pair<std::ifstream *, std::streampos> *vertexpos)
{
    std::string output_filename = output_basename + ".dat";
    std::ofstream out(output_filename, std::ofstream::binary | std::ofstream::trunc);

    std::string offset_filename = output_basename + ".off";
    std::ofstream off(offset_filename, std::ofstream::binary | std::ofstream::trunc);

    uint64_t nv64=num_vertices;
    off.write(reinterpret_cast<char*>(&nv64), sizeof(uint64_t));

    uint64_t num_records_total=0;
    AliasMethodSampler alias_sampler(num_vertices);

    for(UndirectedGraph::vertex_t u=0; u < num_vertices; u++)
    {
        std::ifstream *f = vertexpos[u].first;
        std::streampos pos = vertexpos[u].second;

        f->seekg(pos);
        TreeletTable::treelet_count_t nrecords;
        f->read(reinterpret_cast<char*>(&nrecords), sizeof(TreeletTable::treelet_count_t));

        off.write(reinterpret_cast<char*>(&num_records_total), sizeof(uint64_t));
        add_overflow(num_records_total, nrecords, &num_records_total);
        add_overflow(num_records_total, 1, &num_records_total);

        Treelet t = Treelet::invalid_treelet;
        TreeletTable::treelet_count_t total=0;

        out.write(reinterpret_cast<char*>(&t), sizeof(Treelet));
        out.write(reinterpret_cast<char*>(&total), sizeof(TreeletTable::treelet_count_t));

        for(TreeletTable::treelet_count_t r=0; r < nrecords; r++)
        {
            TreeletTable::treelet_count_t c;
            f->read(reinterpret_cast<char*>(&t), sizeof(Treelet));
            f->read(reinterpret_cast<char*>(&c), sizeof(TreeletTable::treelet_count_t));

            add_overflow(total, c, &total);
            out.write(reinterpret_cast<char*>(&t), sizeof(Treelet));
            out.write(reinterpret_cast<char*>(&total), sizeof(TreeletTable::treelet_count_t));
        }

        alias_sampler.set(u, total);
    }

    off.write(reinterpret_cast<char*>(&num_records_total), sizeof(uint64_t));
    off.close();
    out.close();

    std::string root_sampler_filename = output_basename + ".rts";
    alias_sampler.build();
    alias_sampler.write(root_sampler_filename);

    std::cout << "Processed " << num_vertices << " vertices (wrote " << num_records_total << " records)" << std::endl;
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