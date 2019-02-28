//
// Created by steven on 12/3/16.
//

#include <fstream>
#include <unordered_map>
#include "config.h"

#include "../common/graph/UndirectedGraph.h"
#include "../common/io/PropertyStore.h"
#include "../common/platform/platform.h"
#include "../common/util.h"
#include "TreeletSampler.h"
#include "sampler_opts.h"
#include "OccurrenceSampler.h"
#include "OccurrenceStarSampler.h"
#include "AdaptiveSampler.h"
#include "SampleTable.h"


int main(const int argc, const char** argv)
{
	std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

	try
	{
		sampler_opts opts;
		if (!parse_sampler_args(argc, argv, "motivo-sample", &opts))
			return EXIT_SUCCESS;

		// Read info file
		PropertyStore properties(std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".info");
		const bool store_only_on_0 = properties.get_bool("StoreOnlyOn0", false);
        //Estimate of the total number of colorful treelets
        const uint128_t tot_treelets = properties.get_uint128("TotTreelets", 0) * (store_only_on_0?opts.size:1);

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
		double p = pcol(opts.size, opts.size); // the coloring probability
		std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;



		// FAST STAR SAMPLING
		SampleTable* star_samples = nullptr;
		uint128_t number_of_stars = 0;
		uint64_t number_of_star_samples = 0;
		double time_budget = opts.time_budget;
		if (opts.smart_stars)
		{
			OccurrenceStarSampler star_sampler(&G, opts.size, opts.canonicize);
			number_of_stars = star_sampler.number_of_stars();
			number_of_star_samples = static_cast<uint64_t>(static_cast<double>(opts.number_of_samples) * (1.0 - static_cast<double>(tot_treelets) / (p*static_cast<double>(number_of_stars) + static_cast<double>(tot_treelets))) + 0.5); //FIXME: Sample from binomial distribution?

			std::chrono::time_point < std::chrono::steady_clock > start_time = std::chrono::steady_clock::now();
			std::cout << "Star sampler: sampling " << number_of_star_samples << " stars" << std::endl;

			//Sample stars rooted in the center
			star_samples = star_sampler.sample(number_of_star_samples, opts.threads, &rng, 0.05 * opts.time_budget); //Resulting time budget is infinite if opts.time_budget i
			time_budget *= 0.95;

			std::chrono::duration<double> el = std::chrono::steady_clock::now() - start_time;
			std::cerr << "Star sampler: elapsed " << el.count() << " s\n";
			std::cout << "Star sampler: taken " << star_samples->get_num_samples() << " samples in " << el.count() << " s\n";

		}
		uint64_t nonstar_nsamples = opts.number_of_samples - number_of_star_samples;



		// NAIVE SAMPLER
		std::cout << "Using naive sampler" << std::endl;
		std::chrono::time_point < std::chrono::steady_clock > start_time = std::chrono::steady_clock::now();
		OccurrenceSampler sampler(&G, &ttc, opts.size, opts.vertices, opts.graphlets, opts.canonicize);

		sampler.set_selector(selector, opts.threads);

		SampleTable* samples = sampler.sample(nonstar_nsamples, opts.threads, &rng, time_budget);

		samples->count_spanning_trees(opts.size, build_selector, opts.threads);

		std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
		std::cout << "Naive sampler: taken " << samples->get_num_samples() << " samples in " << elapsed.count() << " s\n";


		if(star_samples)
		{
			std::cout << "merging samples with weights " << static_cast<double>(tot_treelets) / p << "," << static_cast<double>(number_of_stars)  << std::endl;
			//OccurrenceStarSelector::sample takes care of counting spanning stars rooted in the center
			//SampleTable::merge takes care of estimating occurrences and frequencies
			SampleTable *merged = SampleTable::merge(*samples, *star_samples, static_cast<double>(tot_treelets) / p, static_cast<double>(number_of_stars));

			delete samples;
			delete star_samples;

			samples = merged;
		}
		else
        {
            samples->estimateOccurrences(static_cast<double>(tot_treelets) / p);
            samples->estimateFrequencies();
        }

		samples->sort_by_estimate_occ();
		*output << samples->header() << std::endl << *samples << std::endl;

		std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
		std::cerr << "Sampling time: " << delta_t.count() << " s\n";

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
