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
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

/**
 */
OccurrenceSampler::table_t *merge_star_and_nostar_tables(OccurrenceSampler::table_t *t1,
		OccurrenceSampler::table_t *t2, int s1, uint128_t nstars, int s2, uint128_t ntreelets) {
	return nullptr;
}


int main(const int argc, const char** argv) {
	std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

	sampler_opts opts;
	try {
		if (!parse_sampler_args(argc, argv, "motivo-sample", &opts))
			return EXIT_SUCCESS;

		std::ostream* output = &std::cout;
		if (strlen(opts.output_basename) != 0)
			output = new std::ofstream(
					std::string(opts.output_basename) + "." + std::to_string(opts.size)
							+ ".samples", std::ofstream::binary | std::ofstream::trunc);

		// Read info from info file
		std::ifstream infofile;
		opts.store_only_0 = false;
		opts.tot_treelets = 0;
		infofile.open(
				std::string(opts.output_basename) + "." + std::to_string(opts.size) + ".info");
		while (!infofile.eof()) {
			std::string key, val;
			try {
				infofile >> key >> val;
				if (key == "StoreOnlyOn0")
					opts.store_only_0 = (val == "1");
				if (key == "TotTreelets")
					opts.tot_treelets = atoi128(val);
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
				TreeletTable::may_alias>* readers = nullptr;
		TreeletTable** tables = nullptr;
		readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
				TreeletTable::may_alias> [opts.size];
		tables = new TreeletTable*[opts.size];

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

		TreeletSelector* selector = nullptr;
		if (*opts.selective_filename != '\0') {
			selector = new TreeletSelector(opts.selective_filename, opts.size);
			std::cout << "Selectively "
					<< ((selector->get_mode() == TreeletSelector::MODE_INCLUDE) ?
							"sampling only " : "ignoring ") << selector->get_size()
					<< " treelet(s) of the given size" << std::endl;
		}

		std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;

		OccurrenceSampler sampler(&G, &ttc, opts.size, opts.number_of_samples, &rng, opts.vertices,
				opts.graphlets, opts.spanning_trees, opts.footprints, opts.canonicize,
				opts.norejection, opts.text, opts.group, output, opts.threads, selector,
				opts.tot_treelets, opts.store_only_0);

		std::chrono::time_point < std::chrono::steady_clock > tstart =
				std::chrono::steady_clock::now();
		sampler.sample();
		std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
		std::cerr << "Sampling time: " << delta_t.count() << " s\n";
		delete selector;

		for (unsigned int i = 0; i < opts.size; i++)
			delete tables[i];

		delete[] readers;
		delete[] tables;


		// Time to sample stars
		OccurrenceStarSampler star_sampler(&G, opts.size, opts.number_of_samples, &rng,
				opts.canonicize, opts.norejection, opts.group);

		std::cout << to_string(opts.tot_treelets) << " " << star_sampler.get_root_sampler()->get_total_weight();

		Occurrence occ;
		tstart = std::chrono::steady_clock::now();
		OccurrenceSampler::table_t* table = star_sampler.sample(opts.number_of_samples,
				opts.threads);
		delta_t = std::chrono::steady_clock::now() - tstart;
		std::cerr << "Star-based sampling time: " << delta_t.count() << " s\n";
		OccurrenceSampler::table_t::const_iterator it = table->begin();
		while (it != table->end()) {
			std::cout << it->first.text_footprint() << " " << it->second;
			std::cout << " " << SpanningTreeCounter::num_spanning_stars(it->first) << std::endl;
			it++;
		}
		delete table;

		if (strlen(opts.output_basename) != 0)
			delete output;
	}
	catch(std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
