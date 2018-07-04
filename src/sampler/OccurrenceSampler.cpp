//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include <unordered_map>
#include "OccurrenceSampler.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

OccurrenceSampler::table_t* OccurrenceSampler::sample(int num_samples, int number_of_threads) {
	number_of_threads = std::min((int) num_samples / 10, number_of_threads);

	if (number_of_threads == 1) {
		OccurrenceSampler::table_t* count_table = create_table(footprints, vertices);
		do_sample_st(count_table, num_samples);
		return count_table;
	} else {
		OccurrenceSampler::table_t** count_tables = nullptr;
		sequencer_t *sequencer = new sequencer_t(1, num_samples, number_of_threads);
		ConcurrentWriter* writer = nullptr;
		count_tables = new OccurrenceSampler::table_t*[number_of_threads];
		std::thread *worker_threads = new std::thread[number_of_threads];
		int rem_samples = num_samples;
		for (unsigned int i = 0; i < number_of_threads; i++) {
			OccurrenceSampler::table_t* count_table = nullptr;
			count_table = count_tables[i] = create_table(footprints, vertices);
			worker_threads[i] =
					std::thread(
							[this, sequencer, writer, count_table, num_samples] {do_sample_mt(sequencer, writer, count_table, num_samples);});
		}
		for (unsigned int i = 0; i < number_of_threads; i++)
			worker_threads[i].join();
		delete[] worker_threads;
		// Merge the tables
		for (unsigned int i = 1; i < number_of_threads; i++) {
			OccurrenceSampler::table_t::const_iterator it = count_tables[i]->begin();
			while (it != count_tables[i]->end()) {
				(*count_tables[0])[it->first] += it->second;
				it++;
			}
			count_tables[i]->clear();
		}
		OccurrenceSampler::table_t* t0 = count_tables[0];
		for (unsigned int i = 1; i < number_of_threads; i++)
			delete count_tables[i];
		delete[] count_tables;
		// Returned the merged table
		return t0;
	}

}

void OccurrenceSampler::sample(int num_samples) {
	if (number_of_threads == 1) {
		table_t* count_table = nullptr;
		if (group_same)
			count_table = create_table(footprints, vertices);

		do_sample_st(count_table, num_samples);

		if (group_same) {
			write_table_2(count_table);
			delete count_table;
		}
	} else {
		sequencer_t *sequencer = new sequencer_t(1, num_samples, number_of_threads);
		ConcurrentWriter* writer = nullptr;
		table_t** count_tables = nullptr;

		if (group_same)
			count_tables = new table_t*[number_of_threads];
		else
			writer = new ConcurrentWriter(output, 10 * number_of_threads);

		std::thread *worker_threads = new std::thread[number_of_threads];
		for (unsigned int i = 0; i < number_of_threads; i++) {
			table_t* count_table = nullptr;
			if (group_same)
				count_table = count_tables[i] = create_table(footprints, vertices);

			worker_threads[i] =
					std::thread(
							[this, sequencer, writer, count_table, num_samples] {do_sample_mt(sequencer, writer, count_table, num_samples);});
		}

		for (unsigned int i = 0; i < number_of_threads; i++)
			worker_threads[i].join();

		delete[] worker_threads;
		delete writer;
		delete sequencer;

		if (group_same) {
			merge_tables(count_tables);
			write_table_2(count_tables[0]);

			for (unsigned int i = 0; i < number_of_threads; i++)
				delete count_tables[i];
			delete[] count_tables;
		}
	}
	//std::cerr << "Sampled treelets: " << sampled << " (" << static_cast<double>(sampled)/delta_t.count() << " occ/s)" << "\n";
	//std::cerr << "Accepted treelets/graphlets: " << accepted<< " (" << static_cast<double>(accepted)/delta_t.count() << " occ/s)" << "\n";
	//std::cerr << "Rejected treelets/graphlets: " << sampled - accepted << std::endl;
}

OccurrenceSampler::table_t *OccurrenceSampler::create_table(bool footprints = true, bool vertices =
		false) {
	static Occurrence empty_key = Occurrence();
	static Occurrence::OccurrenceHash hasher = Occurrence::OccurrenceHash(footprints, vertices);
	static Occurrence::compare_eq eq = Occurrence::compare_eq(footprints, vertices);

	table_t* table = new table_t(0, hasher, eq);
	table->set_empty_key(empty_key);
	return table;
}

void OccurrenceSampler::merge_tables(OccurrenceSampler::table_t **count_tables) {
	for (unsigned int i = 1; i < number_of_threads; i++) {
		table_t::const_iterator it = count_tables[i]->begin();
		while (it != count_tables[i]->end()) {
			(*count_tables[0])[it->first] += it->second;
			it++;
		}

		count_tables[i]->clear();
	}
}

void OccurrenceSampler::write_table(OccurrenceSampler::table_t *count_table) {
	table_t::const_iterator it = count_table->begin();
	while (it != count_table->end()) {
		if (text) {
			*output << it->second << ":";

			if (footprints)
				*output << it->first.text_footprint() << ";";

			if (spanning_trees_no)
				*output << it->first.number_of_spanning_trees() << ";";

			*output << "\n";
		} else {
			output->write(reinterpret_cast<const char*>(&(it->second)), sizeof(uint64_t));
			if (footprints)
				output->write(it->first.binary_footprint(), Occurrence::binary_footprint_bytes);

			if (spanning_trees_no) {
				uint64_t st = it->first.number_of_spanning_trees();
				output->write(reinterpret_cast<const char*>(&st), sizeof(uint64_t));
			}
		}
		it++;
	}
}

/***
 * Print out the motif sample statistics.
 * count_table maps each graphlet type to its count
 */
void OccurrenceSampler::write_table_2(OccurrenceSampler::table_t *count_table) {

	// 1. Sort the graphlets in nonincreasing order of sampled occurrences
	std::multimap<int64_t, Occurrence> sort_table;
	std::unordered_map<std::string, TreeletTable::treelet_count_t> tc_table;
	uint64_t nsamples = 0;
	double normalized_samples = 0;
	SpanningTreeCounter stc;
	{
		// we check if the selector is excluding just the stars...
		TreeletSelector* ts = sampler.get_selector();
		bool exclude_only_stars = false;
		if (ts && ts->get_treelet_size() == size && ts->get_mode() == TreeletSelector::MODE_EXCLUDE
				&& ts->get_size() == 2) {
			const Treelet* t = ts->get_treelets();
			for (int i = 0; i < ts->get_size(); i++) {
				exclude_only_stars &= t[i].is_star();
			}
		}
		table_t::const_iterator it = count_table->begin();
		while (it != count_table->end()) {
			sort_table.insert(std::make_pair(it->second, it->first));
			nsamples += it->second;
			if (spanning_trees_no) {
				uint64_t st =
						exclude_only_stars ?
								stc.num_spanning_trees_nostars(it->first) :
								stc.num_spanning_trees(it->first, sampler.get_selector());
				tc_table.insert(std::make_pair(std::string(it->first.text_footprint()), st));
				normalized_samples += (double) it->second / (double) st;
			}
			it++;
		}
	}

	// Now tc_table maps each graphlet (its fingerprint) to the number of spanning trees
	// While sort_table maps counts to graphlets
	if (text)
		*output << "motif,raw_count,spanning_trees,estim_freq,estim_occur" << "\n";
	auto it = sort_table.rbegin();
	while (it != sort_table.rend()) {
		if (text) {
			std::string fp = it->second.text_footprint();
			if (footprints)
				*output << fp;
			*output << "," << it->first;
			*output << ",";
			if (spanning_trees_no) {
				// print the number of spanning trees
				*output << static_cast<uint64_t>(tc_table[fp]); // TODO: print 128-bit types!
				// estimate frequency of occurrences
				*output << "," << (double) it->first / (tc_table[fp] * normalized_samples);
				// estimate number of occurrences in the whole graph
				if (tot_treelets > 0) {
					*output << ","
							<< (1.0 * it->first / nsamples)
									* (1.0 * tot_treelets
											/ (tc_table[fp] * (store_only_0 ? 1 : size)))
									/ pcol(size, size);
				}
			}
			*output << "\n";
		} else {
			output->write(reinterpret_cast<const char*>(&(it->first)), sizeof(uint64_t));
			if (footprints)
				output->write(it->second.binary_footprint(), Occurrence::binary_footprint_bytes);

			if (spanning_trees_no) {
				uint64_t st = it->second.number_of_spanning_trees();
				output->write(reinterpret_cast<const char*>(&st), sizeof(uint64_t));
			}
		}
		it++;
	}
}

void OccurrenceSampler::do_sample_st(table_t* count_table, int num_samples) {
	Occurrence occurrence;
	OccurrenceCanonicizer canonicizer(size);

	char* buffer = nullptr;
	if (!group_same)
		buffer = new char[buffer_size];
	char* p = buffer;

	for (uint64_t i = 0; i < num_samples; i++) {
		sample_one(&occurrence);
		if (canonicize)
			canonicizer.canonicize(&occurrence);

		if (group_same)
			(*count_table)[occurrence] += 1;
		else {
			if (p > buffer + buffer_size - max_occurrence_size) {
				output->write(buffer, p - buffer);
				p = buffer;
			}

			p = write(&occurrence, p);
		}
	}

	if (p != buffer)
		output->write(buffer, p - buffer);

	delete[] buffer;
}

void OccurrenceSampler::do_sample_mt(sequencer_t *sequencer, ConcurrentWriter *writer,
		table_t* count_table, int num_samples) {
	Occurrence occurrence;
	OccurrenceCanonicizer canonicizer(size);

	char* buffer = nullptr;
	if (!group_same)
		buffer = new char[buffer_size];
	char* p = buffer;

	while (true) {
		sequencer_t::sequence_batch_t batch = sequencer->next_batch();
		if (batch.from >= batch.to)
			break;

		for (uint64_t i = batch.from; i < batch.to; i++) {
			sample_one(&occurrence);
			if (canonicize)
				canonicizer.canonicize(&occurrence);

			if (group_same)
				(*count_table)[occurrence] += 1;
			else {
				if (p > buffer + buffer_size - max_occurrence_size) {
					writer->write(buffer, static_cast<std::size_t>(p - buffer));
					p = buffer = new char[buffer_size];
				}

				p = write(&occurrence, p);
			}
		}
	}

	if (p != buffer)
		writer->write(buffer, static_cast<std::size_t>(p - buffer));
	else
		delete[] buffer;
}

char* OccurrenceSampler::write(Occurrence *occurrence, char* buf) {
	if (text) {
		*(buf++) = '1';
		*(buf++) = ':';

		if (footprints) {
			strcpy(buf, occurrence->text_footprint());
			buf += Occurrence::text_footprint_bytes;
			*(buf++) = ';';
		}

		if (spanning_trees_no)
			buf += sprintf(buf, "%" PRIu64 ";", occurrence->number_of_spanning_trees());

		if (vertices) {
			const UndirectedGraph::vertex_t* verts = occurrence->vertices();
			for (unsigned int i = 0; i < size; i++) {
				buf += sprintf(buf, "%" PRIu32, verts[i]);
				(*buf++) = (i == size - 1) ? ';' : ' ';
			}
		}

		(*buf++) = '\n';
	} else {
		if (footprints) {
			memcpy(buf, occurrence->binary_footprint(), Occurrence::binary_footprint_bytes);
			buf += Occurrence::binary_footprint_bytes;
		}

		if (spanning_trees_no) {
			uint64_t st = occurrence->number_of_spanning_trees();
			memcpy(buf, &st, sizeof(uint64_t));
			buf += sizeof(uint64_t);
		}

		if (vertices) {
			memcpy(buf, occurrence->vertices(), sizeof(UndirectedGraph::vertex_t) * size);
			buf += sizeof(UndirectedGraph::vertex_t) * size;
		}
	}

	return buf;
}

OccurrenceSampler::OccurrenceSampler(UndirectedGraph *graph, TreeletTableCollection *ttc,
		unsigned int size, Random *rng, bool vertices, bool graphlets, bool spanning_trees_no,
		bool footprints, bool canonicize, bool no_rejection, bool text, bool group_same,
		std::ostream *out, unsigned int number_of_threads, TreeletSelector* selector,
		uint128_t tot_treelets, bool store_only_0) :
		graph(graph), ttc(ttc), size(size), rng(rng), vertices(vertices), graphlets(graphlets), spanning_trees_no(
				spanning_trees_no), footprints(footprints), canonicize(canonicize), no_rejection(
				no_rejection), text(text), group_same(group_same), output(out), number_of_threads(
				number_of_threads), sampler(graph, ttc, size, rng), tot_treelets(
				tot_treelets), store_only_0(store_only_0) {
	if (number_of_threads == 0)
		throw std::runtime_error("Invalid number of threads");

	sampler.set_selector(selector, number_of_threads);
}

