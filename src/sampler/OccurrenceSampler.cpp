//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include <unordered_map>
#include "OccurrenceSampler.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

void OccurrenceSampler::sample_one(Occurrence *occurrence) {
	UndirectedGraph::vertex_t sampled_vertices[16];
	UndirectedGraph::vertex_t root = sampler.sample_root();
	assert(root < graph->number_of_vertices());
	Treelet t = sampler.sample_treelet(root);

	while (true) {
		if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
				{
#ifndef NDEBUG
			bool success =
#endif
					sampler.sample_rooted_occurrence(t, root, sampled_vertices); //FIXME: Handle case in which there are no treelets
			assert(success);
		}

		if (graphlets) {
			new (occurrence) Occurrence(size, graph, sampled_vertices);

			if (!no_rejection
					&& rng->random_uint<uint64_t>(0, occurrence->number_of_spanning_trees() - 1)
							!= 0)
				continue; //Rejection
		} else
			new (occurrence) Occurrence(t, sampled_vertices);

		break;
	}
}

void OccurrenceSampler::sample() {
	if (number_of_threads == 1) {
		table_t* count_table = nullptr;
		if (group_same)
			count_table = create_table();

		do_sample_st(count_table);

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
				count_table = count_tables[i] = create_table();

			worker_threads[i] =
					std::thread(
							[this, sequencer, writer, count_table] {do_sample_mt(sequencer, writer, count_table);});
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

OccurrenceSampler::table_t *OccurrenceSampler::create_table() {
	static Occurrence empty_key = Occurrence();
	static OccurrenceHash hasher = OccurrenceHash(footprints, vertices);
	static OccurrenceEquality eq = OccurrenceEquality(footprints, vertices);

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
		table_t::const_iterator it = count_table->begin();
		while (it != count_table->end()) {
			sort_table.insert(std::make_pair(it->second, it->first));
			nsamples += it->second;
			if (spanning_trees_no) {
				uint64_t st = stc.num_spanning_trees(it->first, sampler.get_selector());
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
//				*output << "," << nsamples;
//				*output << "," << pcol(size, size);
//				*output << "," << size;
//				*output << "," << to_string(tot_treelets);
//				*output << "," << ((double)tot_treelets / tc_table[fp]);
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

void OccurrenceSampler::do_sample_st(table_t* count_table) {
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
		table_t* count_table) {
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
		unsigned int size, uint64_t num_samples, Random *rng, bool vertices, bool graphlets,
		bool spanning_trees_no, bool footprints, bool canonicize, bool no_rejection, bool text,
		bool group_same, std::ostream *out, unsigned int number_of_threads,
		TreeletSelector* selector, uint128_t tot_treelets, bool store_only_0) :
		graph(graph), ttc(ttc), size(size), num_samples(num_samples), rng(rng), vertices(vertices), graphlets(
				graphlets), spanning_trees_no(spanning_trees_no), footprints(footprints), canonicize(
				canonicize), no_rejection(no_rejection), text(text), group_same(group_same), output(
				out), number_of_threads(number_of_threads), sampler(graph, ttc, size, rng,
				selector), tot_treelets(tot_treelets), store_only_0(store_only_0) {
	if (number_of_threads == 0)
		throw std::runtime_error("Invalid number of threads");
}
