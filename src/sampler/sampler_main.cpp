//
// Created by steven on 12/3/16.
//

#include <fstream>
#include <unordered_map>
#include "../common/graph/UndirectedGraph.h"
#include "TreeletSampler.h"
#include "sampler_opts.h"
#include "OccurrenceSampler.h"
#include "OccurrenceStarSampler.h"
#include "AdaptiveSampler.h"
#include "SampleTable.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

int main(const int argc, const char** argv) {
	std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

	try {
		sampler_opts opts;
		if (!parse_sampler_args(argc, argv, "motivo-sample", &opts))
			return EXIT_SUCCESS;

		// Read info file
		bool store_only_on_0 = false;
		uint128_t tot_treelets = 0; // the total number of colored treelets
		std::ifstream infofile;
		std::string infofile_name = std::string(opts.tables_basename) + "."
				+ std::to_string(opts.size) + ".info";
		std::cout << "Reading info file " << infofile_name << std::endl;
		infofile.open(infofile_name);
		while (!infofile.eof()) {
			std::string key, val;
			try {
				infofile >> key >> val;
				if (key == "StoreOnlyOn0")
					store_only_on_0 = (val == "1");
				if (key == "TotTreelets")
					tot_treelets = atoi128(val); //FIXME
			}
			catch (std::exception &e) {
				std::cerr << "Error reading info file!" << std::endl;
			}
		}
		infofile.close();

		UndirectedGraph G(opts.graph);
		G.prefault();
		std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and "
				<< G.number_of_edges() << " edges" << std::endl;

		std::cerr << "Loading tables and root sampler" << std::endl;
		TreeletTableCollection ttc;
		CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
				TreeletTable::may_alias> *readers = nullptr;
		TreeletTable **tables = nullptr;
		readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
				TreeletTable::may_alias> [opts.size];
		tables = new TreeletTable *[opts.size];

		for (unsigned int i = 0; i < opts.size; i++) {
			readers[i].open(
					std::string(opts.tables_basename) + "." + std::to_string(i + 1) + ".dtz");
			readers[i].prefault(0, G.number_of_vertices() - 1);
			tables[i] = new TreeletTable(&readers[i]);
			ttc.add(tables[i]);
		}
		tables[opts.size - 1]->load_root_sampler(
				std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".rts");

		Random rng(opts.seed);
		std::cerr << "Using seed " << rng.get_seed() << std::endl;

		/**
		 * Treelet selector using only size-k for sampling
		 */
		TreeletSelector *selector = nullptr;
		TreeletSelector *full_selector = nullptr;
		if (*opts.selective_filename != '\0') {
			full_selector = new TreeletSelector(opts.selective_filename);
			selector = new TreeletSelector(opts.selective_filename, opts.size);
			std::cout << "Selectively "
					<< ((selector->get_mode() == TreeletSelector::MODE_INCLUDE) ?
							"sampling only " : "ignoring ") << selector->get_size()
					<< " treelet(s) of the given size" << std::endl;
		}

		std::ostream *output = &std::cout;
		if (strlen(opts.output_basename) != 0)
			output = new std::ofstream(std::string(opts.output_basename) + +".csv",
					std::ofstream::binary | std::ofstream::trunc);

		/*********************
		 ** ACTUAL SAMPLING **
		 *********************/
		std::chrono::time_point < std::chrono::steady_clock > tstart =
				std::chrono::steady_clock::now();
		double p = pcol(opts.size, opts.size); // the coloring probability
		std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;

		// 1. FAST STAR SAMPLING
		SampleTable* star_samples = nullptr;
		uint128_t nstars = 0;
		uint64_t star_nsamples = 0;
		double time_bud =
				opts.time_budget < std::numeric_limits<double>::infinity() && opts.time_budget > 0 ?
						opts.time_budget : std::numeric_limits<double>::infinity();
		if (opts.smart_stars) { // let's see...
			OccurrenceStarSampler star_sampler(&G, opts.size, opts.threads, opts.canonicize);
			nstars = star_sampler.number_of_stars();
			star_nsamples = static_cast<uint64_t>(opts.number_of_samples
					* (1 - tot_treelets / (p * nstars + tot_treelets)) + 0.5);
			if (star_nsamples > 0 || (opts.number_of_samples == 0 && time_bud > 0)) { // fast star sampling
				std::chrono::time_point < std::chrono::steady_clock > sampstart =
						std::chrono::steady_clock::now();
				std::cout << "star sampler" << std::endl;
				star_samples = star_sampler.sample(star_nsamples, &rng, 0.05 * time_bud);
				time_bud *= 0.95; // 5% stars, 95% non-stars
				TreeletSelector star_selector = TreeletSelector::get_star_selector(opts.size,
						TreeletSelector::MODE_INCLUDE);
//				star_samples = new SampleTable(star_occurrences, star_nsamples, &star_selector);
//				delete[] star_occurrences;
				star_samples->estimateOccurrences((double) nstars, opts.size, store_only_on_0);
				star_samples->estimateFrequencies();
				std::chrono::duration<double> el = std::chrono::steady_clock::now() - sampstart;
				std::cerr << "star sampler: elapsed " << el.count() << " s\n";
				std::cout << "star sampler: taken " << star_samples->get_num_samples()
						<< " samples in " << el.count() << " s\n";
			}
		}
		uint64_t nonstar_nsamples = opts.number_of_samples - star_nsamples;

		// 2. SAMPLING THE REST
		if (opts.adaptive) {
			std::cout << "adaptive sampler" << std::endl;
			std::chrono::time_point < std::chrono::steady_clock > sampstart =
					std::chrono::steady_clock::now();
			AdaptiveSampler sampler(&G,
					std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".dtz",
					nullptr, opts.size, &ttc, store_only_on_0);
			SampleTable* samples = sampler.sample(nonstar_nsamples, opts.threads, &rng, time_bud);
			std::chrono::duration<double> el = std::chrono::steady_clock::now() - sampstart;
			std::cout << "adaptive sampler: taken " << samples->get_num_samples() << " samples in "
					<< el.count() << " s\n";
			if (star_samples) {
				double w = (tot_treelets / p) / (nstars + tot_treelets / p), w0 = 1 - w;
				std::cout << "merging samples with weights " << w0 << "," << w << std::endl;
				SampleTable merged = SampleTable::average(*samples, *star_samples, w, w0);
				std::cout << *star_samples << std::endl;
				std::cout << *samples << std::endl;
				delete samples, star_samples;
				merged.sort_by_estimate_occ();
				*output << merged.header() << "\n" << merged << std::endl;
			} else {
				samples->sort_by_estimate_occ();
				std::cout << "NOT merging samples" << std::endl;
				*output << samples->header() << "\n" << *samples << std::endl;
				delete samples;
			}
		} else {
			std::cout << "naive sampler" << std::endl;
			std::chrono::time_point < std::chrono::steady_clock > sampstart =
					std::chrono::steady_clock::now();
			OccurrenceSampler sampler(&G, &ttc, opts.size, opts.vertices, opts.graphlets,
					opts.canonicize, opts.norejection);
			sampler.set_selector(selector, opts.threads, full_selector);
			SpanningTreeCounter *stc = new SpanningTreeCounter();
			if (!opts.sptrees_file.empty())
				stc->read_from_file(opts.sptrees_file);
			sampler.setSpanningTreeCounter(stc);
			SampleTable* samples = sampler.sample(nonstar_nsamples, opts.threads, &rng, time_bud);
//			samples->update_spanning_trees(full_selector);
			if (!opts.sptrees_file.empty())
				stc->save_to_file(opts.sptrees_file);
			samples->estimateOccurrences(tot_treelets / p, opts.size, store_only_on_0);
			samples->estimateFrequencies();
			std::chrono::duration<double> el = std::chrono::steady_clock::now() - sampstart;
			std::cout << "naive sampler: taken " << samples->get_num_samples() << " samples in "
					<< el.count() << " s\n";
			if (star_samples != nullptr) {
				SampleTable merged = SampleTable::merge(*samples, *star_samples, tot_treelets / p,
						nstars);
				delete samples, star_samples;
				merged.sort_by_estimate_occ();
				*output << merged.header() << std::endl << merged << std::endl;
			} else {
				samples->sort_by_estimate_occ();
				*output << samples->header() << std::endl << *samples << std::endl;
			}
		}

		std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
		std::cerr << "Sampling time: " << delta_t.count() << " s\n";

		for (unsigned int i = 0; i < opts.size; i++)
			delete tables[i];
		delete[] readers;
		delete[] tables;

		delete selector;
//		delete all_size_selector;
		if (strlen(opts.output_basename) != 0)
			delete output;

	}
	catch(std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
