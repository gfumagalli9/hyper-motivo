//
// Created by steven on 9/11/17.
//

#include <thread>
#include <cinttypes>
#include <unordered_map>
#include "OccurrenceSampler.h"
#include "../common/SpanningTreeCounter.h"
#include "../common/common.h"

OccurrenceSampler::table_t* OccurrenceSampler::sample(const unsigned int num_samples)
{
	unsigned int nthreads = std::min(num_samples / 10, number_of_threads);

	if (nthreads <= 1)
	{
		OccurrenceSampler::table_t* count_table = create_table(footprints, vertices);
		do_sample_st(count_table, num_samples);
		return count_table;
	}
	else
    {
		OccurrenceSampler::table_t** count_tables = nullptr;
		auto *sequencer = new sequencer_t(1, num_samples, nthreads);
		ConcurrentWriter* writer = nullptr;
		count_tables = new OccurrenceSampler::table_t*[nthreads];

		auto worker_threads = new std::thread[nthreads];
		for (unsigned int i = 0; i < nthreads; i++)
		{
			OccurrenceSampler::table_t* count_table = nullptr;
			count_table = count_tables[i] = create_table(footprints, vertices);
			worker_threads[i] = std::thread([this, sequencer, writer, count_table, num_samples] {do_sample_mt(sequencer, writer, count_table, num_samples);});
		}

		for (unsigned int i = 0; i < nthreads; i++)
			worker_threads[i].join();

		delete[] worker_threads;

		// Merge the tables
        OccurrenceSampler::table_t* t0 = count_tables[0];
        for (unsigned int i = 1; i < nthreads; i++)
		{
			OccurrenceSampler::table_t::const_iterator it = count_tables[i]->begin();
			while (it != count_tables[i]->end())
			{
				(*t0)[it->first] += it->second;
				it++;
			}
			//count_tables[i]->clear();
            delete count_tables[i];
		}

		delete[] count_tables;

		// Return the merged table
		return t0;
	}

}

OccurrenceSampler::table_t *OccurrenceSampler::create_table(bool footprints, bool vertices)
{
	static Occurrence empty_key = Occurrence();
	static Occurrence::OccurrenceHash hasher = Occurrence::OccurrenceHash(footprints, vertices);
	static Occurrence::compare_eq eq = Occurrence::compare_eq(footprints, vertices);

	table_t* table = new table_t(0, hasher, eq);
	table->set_empty_key(empty_key);
	return table;
}


void OccurrenceSampler::do_sample_st(table_t* count_table, int num_samples)
{
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
		else
        {
			if (p > buffer + buffer_size - max_occurrence_size)
			{
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

void OccurrenceSampler::do_sample_mt(sequencer_t *sequencer, ConcurrentWriter *writer, table_t* count_table, int num_samples) {
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
			else
            {
				if (p > buffer + buffer_size - max_occurrence_size)
				{
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
		std::ostream *out, unsigned int number_of_threads, TreeletSelector* selector) :
		graph(graph), ttc(ttc), size(size), rng(rng), vertices(vertices), graphlets(graphlets),
		spanning_trees_no(spanning_trees_no), footprints(footprints), canonicize(canonicize),
		no_rejection(no_rejection), text(text), group_same(group_same), output(out),
		number_of_threads(number_of_threads), sampler(graph, ttc, size, rng)
{

	if (number_of_threads == 0)
		throw std::runtime_error("Invalid number of threads");

	sampler.set_selector(selector, number_of_threads);
}

