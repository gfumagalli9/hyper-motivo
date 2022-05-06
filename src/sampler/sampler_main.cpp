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

#include <fstream>
#include "config.h"

#include "../common/graph/UndirectedGraph.h"
#include "../common/io/PropertyStore.h"
#include "../common/platform/platform.h"
#include "../common/util.h"
#include "sampler_opts.h"
#include "OccurrenceSampler.h"
#include "OccurrenceStarSampler.h"
#include "AdaptiveSampler.h"
#include "SampleTable.h"

int main(const int argc, const char** argv)
{
    std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << "\n" << MOTIVO_COPYRIGHT_NOTICE << std::endl;

    try
    {
        sampler_opts opts{};
        if (!parse_sampler_args(argc, argv, "motivo-sample", &opts))
            return EXIT_SUCCESS;

        // Read info files
        PropertyStore properties(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".info");
        const bool store_only_on_0 = properties.get_bool("StoreOnlyOn0", false);
        //Estimate of the total number of colorful treelets
        const uint128_t tot_colorful_treelets = properties.get_uint128("TotTreelets", 0) * (store_only_on_0?opts.size:1);

        PropertyStore properties1(std::string(opts.tables_basename) + ".1.info");
        const uint8_t colors = properties1.get_uint8("NumberOfColors", 0);
        double p = properties.get_double("ColoringProbability", pcol(opts.size, colors));


        //Load graph
        UndirectedGraph G(opts.graph);
        G.prefault();
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges() << " edges" << std::endl;

        //Load tables
        std::cerr << "Loading tables and root sampler" << std::endl;
        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias> *readers = nullptr;
        TreeletTable **tables = nullptr;
        readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias> [opts.size];
        tables = new TreeletTable*[opts.size];

        for (unsigned int i = 0; i < opts.size; i++)
        {
            readers[i].open(std::string(opts.tables_basename) + "." + std::to_string(i + 1) + ".dtz");
            readers[i].prefault(0, G.number_of_vertices() - 1);
            tables[i] = new TreeletTable(&readers[i]);
            ttc.add(tables[i]);
        }
        tables[opts.size - 1]->load_root_sampler(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".rts");

        Random rng(opts.seed);
        std::cerr << "Using seed " << rng.get_seed() << std::endl;

        //Load sampling selector
        TreeletStructureSelector *selector = nullptr;
        if (*opts.selective_filename != '\0')
            selector = new TreeletStructureSelector(TreeletStructureSelector(opts.selective_filename).restrict_to_sizes(opts.size,opts.size));

        //Load builder selector
        TreeletStructureSelector *build_selector = nullptr;
        if (*opts.selective_build_filename != '\0')
            build_selector = new TreeletStructureSelector(TreeletStructureSelector(opts.selective_build_filename));


        std::ostream *output = &std::cout;
        if (strlen(opts.output_basename) != 0)
            output = new std::ofstream(std::string(opts.output_basename) + +".csv", std::ofstream::binary | std::ofstream::trunc);

        /*********************
         ** ACTUAL SAMPLING **
         *********************/
        std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
        std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;


        // FAST STAR SAMPLING
        SampleTable* star_samples = nullptr;  //TODO: If we don't care about vertices we can trivially fill this
        double number_of_stars_rooted_in_center = 0;
        uint64_t number_of_star_samples = 0;
        double time_budget = opts.time_budget;
        if (opts.smart_stars)
        {
            OccurrenceStarSampler star_sampler(&G, opts.size, opts.canonicize);
            number_of_stars_rooted_in_center = static_cast<double>(star_sampler.number_of_stars());

            if(opts.auto_number_of_stars)
            {

	            //FIXME: Sample from binomial distribution?
                //If we had counted stars, the total number of colorful treelets (in expectation) would be tot_colorful_treelets + number_of_stars_rooted_in_center * opts.size * p
                //We sample proportionally to the fraction of stars w.r.t. this number of treelets
                number_of_star_samples = static_cast<uint64_t>(static_cast<double>(opts.number_of_samples) * number_of_stars_rooted_in_center*opts.size / (static_cast<double>(tot_colorful_treelets)/p + number_of_stars_rooted_in_center*opts.size) + 0.5);

                if(opts.adaptive) // for AGS it does not make much sense to sample stars proportionally to their number
		            number_of_star_samples = (number_of_star_samples <= opts.number_of_samples/2) ? number_of_star_samples : opts.number_of_samples/2;
            }
            else
                number_of_star_samples = opts.number_of_star_samples;

            std::chrono::time_point < std::chrono::steady_clock > start_time = std::chrono::steady_clock::now();
            std::cout << "Star sampler: sampling " << number_of_star_samples << " stars" << std::endl;

            //Sample stars rooted in the center
            star_samples = star_sampler.sample(number_of_star_samples, opts.threads, &rng, 0.05 * opts.time_budget); //Resulting time budget is infinite if opts.time_budget i

            std::chrono::duration<double> el = std::chrono::steady_clock::now() - start_time;
            std::cout << "Star sampler: took " << star_samples->get_num_samples() << " samples in " << el.count() << " s\n";
	    number_of_star_samples = star_samples->get_num_samples();
            time_budget = time_budget - el.count();
	    
            if(opts.group || opts.spanning_trees)
                star_samples->sort_by_footprint();

            if(opts.group)
                star_samples->group_by_footprint();

            if(opts.spanning_trees)
                star_samples->count_rooted_spanning_stars();
        }

        uint64_t nonstar_nsamples = opts.number_of_samples - number_of_star_samples;

        SampleTable *samples = nullptr;
        if(!opts.adaptive)
        {
            // NAIVE SAMPLER
            std::cout << "Using naive sampler" << std::endl;
            std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();
            OccurrenceSampler sampler(&G, &ttc, opts.size, opts.vertices, opts.graphlets, opts.canonicize, opts.treelet_buffer_size, opts.treelet_buffer_degree);

            sampler.set_selector(selector, opts.threads);

            samples = sampler.sample(nonstar_nsamples, opts.threads, &rng, time_budget);

            std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
            std::cout << "Naive sampler: took " << samples->get_num_samples() << " samples in " << elapsed.count() << " s\n";

            if(opts.group || opts.spanning_trees)
                samples->sort_by_footprint();

            if(opts.group)
                samples->group_by_footprint();

            if(opts.spanning_trees)
                samples->count_rooted_spanning_trees(build_selector, opts.threads);


            if(opts.estimate_occurrences)
            {
                samples->estimate_occurrences(static_cast<double>(tot_colorful_treelets) / p);

                if(star_samples)
                {
                    //At this point star_samples are already grouped by footprint
                    std::cout << "Merging samples with weights " << static_cast<double>(tot_colorful_treelets)/p  << " and " << number_of_stars_rooted_in_center  << std::endl;
                    //SampleTable::merge takes care of estimating occurrences and frequencies
                    SampleTable *merged = SampleTable::merge(*samples, *star_samples, static_cast<double>(tot_colorful_treelets)/p , number_of_stars_rooted_in_center);

                    delete samples;
                    delete star_samples;

                    samples = merged;
                    star_samples = nullptr;
                }
                else
                    samples->estimate_frequencies();


                samples->sort_by_estimate_occurrences();
                *output << SampleTable::header << "\n" << *samples;
            }
            else
            {
                *output << SampleTable::header << "\n";

                if(star_samples)
                    *output << *star_samples;

                *output << *samples;
            }
        }
        else
        {
            // ADAPTIVE SAMPLER
            std::cout << "Using adaptive sampler" << std::endl;
            std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

            AdaptiveSampler sampler(&G, &ttc, opts.size, opts.threads, store_only_on_0, opts.treelet_buffer_size, opts.treelet_buffer_degree);
            samples = sampler.sample(nonstar_nsamples, &rng, time_budget, build_selector);

            std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
            std::cout << "Adaptive sampler: took " << samples->get_num_samples() << " samples in " << elapsed.count() << " s\n";

            //There is no need to bother counting the spanning trees w.r.t. the build selector if we are going to average with star samples
            if(opts.spanning_trees && star_samples==nullptr) //FIXME: Can we compute this in the adaptive sampler itself?
                samples->count_rooted_spanning_trees(build_selector, opts.threads);

            //AdaptiveSampler already estimates occurrences
            samples->rescale_occurrences(1/p);

            if(star_samples)
            {
                double w = (static_cast<double>(tot_colorful_treelets) / p) / (opts.size*number_of_stars_rooted_in_center + static_cast<double>(tot_colorful_treelets) / p);

                std::cout << "Averaging samples with weights " << (1-w) << " and " << w << std::endl;
                //SampleTable::average takes care of estimating occurrences and frequencies
                samples->sort_by_footprint();
                star_samples->estimate_occurrences(number_of_stars_rooted_in_center);

                //std::cerr << "Samples \n" << *samples << "\n Star Samples\n" << *star_samples << std::endl;

                SampleTable* merged = SampleTable::weighted_average(*samples, *star_samples, 1-w, w);

                delete samples;
                delete star_samples;

                star_samples = nullptr;
                samples = merged;
            }
            else
                samples->estimate_frequencies();

            samples->sort_by_estimate_occurrences();
            *output << SampleTable::header << "\n" << *samples;
        }

        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
        std::cerr << "Sampling time: " << delta_t.count() << " s\n";

        delete star_samples;
        delete samples;

        for (unsigned int i = 0; i < opts.size; i++)
            delete tables[i];

        delete[] readers;
        delete[] tables;

        delete selector;
        delete build_selector;

        if (strlen(opts.output_basename) != 0)
            delete output;

    }
    catch(std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
