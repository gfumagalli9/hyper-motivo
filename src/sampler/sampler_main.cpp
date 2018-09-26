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

int main(const int argc, const char** argv)
{
	std::cerr << "This is motivo-sample. Version: " << MOTIVO_VERSION_STRING << std::endl;

	try
    {
        sampler_opts opts;
        if (!parse_sampler_args(argc, argv, "motivo-sample", &opts))
            return EXIT_SUCCESS;

        // Read info file
        bool store_only_on_0 = false;
        uint128_t tot_treelets = 0; // the total number of colored treelets
        std::ifstream infofile;
        std::string infofile_name = std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".info";
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
        std::cerr << "Loaded graph with " << G.number_of_vertices() << " vertices and " << G.number_of_edges()
                  << " edges" << std::endl;


        std::cerr << "Loading tables and root sampler" << std::endl;
        TreeletTableCollection ttc;
        CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias> *readers = nullptr;
        TreeletTable **tables = nullptr;
        readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias, TreeletTable::may_alias>[opts.size];
        tables = new TreeletTable *[opts.size];

        for (unsigned int i = 0; i < opts.size; i++) {
            readers[i].open(std::string(opts.tables_basename) + "." + std::to_string(i + 1) + ".dtz");
            readers[i].prefault(0, G.number_of_vertices() - 1);
            tables[i] = new TreeletTable(&readers[i]);
            ttc.add(tables[i]);
        }
        tables[opts.size - 1]->load_root_sampler(
                std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".rts");

        Random rng(opts.seed);
        std::cerr << "Using seed " << rng.get_seed() << std::endl;

        TreeletSelector *selector = nullptr;
        if (*opts.selective_filename != '\0') {
            selector = new TreeletSelector(opts.selective_filename, opts.size);
            std::cout << "Selectively "
                      << ((selector->get_mode() == TreeletSelector::MODE_INCLUDE) ? "sampling only " : "ignoring ")
                      << selector->get_size()
                      << " treelet(s) of the given size" << std::endl;
        }

        std::ostream *output = &std::cout;
        if (strlen(opts.output_basename) != 0)
            output = new std::ofstream(std::string(opts.output_basename) + +".csv",  std::ofstream::binary | std::ofstream::trunc);

        std::chrono::time_point<std::chrono::steady_clock> tstart = std::chrono::steady_clock::now();
        if (opts.adaptive)
        {
        	std::cout << "sampler: adaptive" << std::endl;
            AdaptiveSampler ad_sampler(&G, std::string(opts.tables_basename) + "." + std::to_string(opts.size) + ".dtz",
                                       nullptr, opts.size, &ttc, store_only_on_0);
            Occurrence occ;
            SampleTable st = ad_sampler.sample(opts.number_of_samples, opts.threads, &rng);
            st.sort_by_estimate_occ();
            *output << st.header() << std::endl;
            *output << st << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "Sampling using " << opts.threads << " thread(s)" << std::endl;
            OccurrenceSampler sampler(&G, &ttc, opts.size, opts.vertices, opts.graphlets, opts.canonicize, opts.norejection);
            sampler.set_selector(selector, opts.threads);

            double p = pcol(opts.size, opts.size);

            if (!opts.smart_stars)
            {
            	std::cout << "sampler: standard" << std::endl;
                Occurrence*table = sampler.sample(opts.number_of_samples, opts.threads, &rng);
                SampleTable st = SampleTable(table, opts.number_of_samples, selector);
                delete[] table;
                st.estimateOccurrences(tot_treelets / p, store_only_on_0);
                st.sort_by_estimate_occ();
                *output << st.header() << std::endl << st << std::endl;
            }
            else
            {
            	std::cout << "sampler: fast stars" << std::endl;
                OccurrenceStarSampler star_sampler(&G, opts.size, opts.threads, opts.canonicize);
                const double nstars = star_sampler.number_of_stars();

                //FIXME: Throw binomial rv
                uint64_t star_nsamples = static_cast<uint64_t>(opts.number_of_samples * (1 - tot_treelets/(p * nstars + tot_treelets)) + 0.5);
                uint64_t nonstar_nsamples = opts.number_of_samples - star_nsamples;

                Occurrence *occurrences = sampler.sample(nonstar_nsamples, opts.threads, &rng);
                SampleTable st0(occurrences, nonstar_nsamples, selector);
                delete[] occurrences;

                st0.estimateOccurrences(tot_treelets / p, store_only_on_0);

                if(star_nsamples!=0)
                {
                    Occurrence *star_occurrences = star_sampler.sample(star_nsamples, &rng);

                    TreeletSelector star_selector = TreeletSelector::get_star_selector(opts.size, TreeletSelector::MODE_INCLUDE);
                    SampleTable st(star_occurrences, star_nsamples, &star_selector);
                    delete[] star_occurrences;

                    //FIXME:!!! This seemed wrong!!! Is p*nstars just nstars?
                    //Notice also that the second argument was an (implicitly converted) bool. Probably not intended
                    //It was st.estimateOccurrences(p * nstars, store_only_on_0);
                    st.estimateOccurrences(nstars, opts.size, store_only_on_0);

                    SampleTable merged = SampleTable::merge(st0, st, tot_treelets / p, nstars);
                    merged.sort_by_estimate_occ();
                    *output << merged.header() << "\n" << merged << std::endl;
                }
                else
                {
                    st0.sort_by_estimate_occ();
                    *output << st0.header() << "\n" << st0 << std::endl;
                }
            }
        }

		std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
		std::cerr << "Sampling time: " << delta_t.count() << " s\n";

		for (unsigned int i = 0; i < opts.size; i++)
			delete tables[i];
		delete[] readers;
		delete[] tables;

		delete selector;
		if (strlen(opts.output_basename) != 0)
			delete output;

	}
	catch(std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
