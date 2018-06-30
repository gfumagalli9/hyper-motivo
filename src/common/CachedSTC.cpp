/*
 * CachedSTC.cpp
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#include "CachedSTC.h"

CachedSTC::CachedSTC() {
	// TODO Auto-generated constructor stub
//	table.set_empty_key(Occurrence());
}

CachedSTC::~CachedSTC() {
	// TODO Auto-generated destructor stub
}

struct vertex_info {
	char* ptr;
	uint64_t count = 0;
};

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices,
		vertex_info* info) {
	uint64_t num_treelet_count_pairs = 0;
	TreeletTable::treelet_count_t num_occ_treelet = 0;
	uint128_t num_occ_total = 0;
	uint128_t num_occ_max = 0;
	bool num_occ_total_overflow = false;

	std::string output_filename = output_basename + ".dtz";
	CompressedRecordFileWriter writer(output_filename, num_vertices);

	for (UndirectedGraph::vertex_t u = 0; u < num_vertices; u++) {
		num_treelet_count_pairs += info[u].count;
		TreeletTable::treelet_count_pair *to_write =
				new TreeletTable::treelet_count_pair[info[u].count + 1];
		TreeletTable::treelet_count_pair *p = to_write;
		p->treelet = Treelet::invalid_treelet;
		p->count = 0;
		for (TreeletTable::treelet_count_t i = 0; i < info[u].count; i++) {
			p++;
			memcpy(p, info[u].ptr, sizeof(TreeletTable::treelet_count_pair));
			if (num_occ_treelet < p->count)
				num_occ_treelet = p->count;
			p->count += (p - 1)->count;
			info[u].ptr += sizeof(TreeletTable::treelet_count_pair);
		}
		writer.write_record(reinterpret_cast<char*>(to_write),
				(info[u].count + 1) * sizeof(TreeletTable::treelet_count_pair), 0);
		if (add_overflow(num_occ_total, p->count, &num_occ_total))
			num_occ_total_overflow = true;
		if (p->count > num_occ_max)
			num_occ_max = p->count;
		delete[] to_write;
	}
	writer.close();
}

/**
 * Copied from merger.cpp
 */
void merge(const std::vector<std::string>& count_filenames, const std::string& output_basename) {
	const unsigned long no_files = count_filenames.size();
	UndirectedGraph::vertex_t num_vertices = 0;
	std::pair<char*, size_t>* cnt_map = new std::pair<char*, size_t>[no_files];
	FILE** count_files = new FILE*[no_files];
	vertex_info* info = nullptr;
	std::vector<bool> seen_vertices;

	for (unsigned int i = 0; i < no_files; i++) {
		const std::string &filename = count_filenames[i];
		count_files[i] = fopen(filename.c_str(), "rb");
		if (count_files[i] == NULL)
			throw std::runtime_error("Unable to open file " + filename);
		UndirectedGraph::vertex_t nv;
		fread(&nv, sizeof(UndirectedGraph::vertex_t), 1, count_files[i]);
		if (i == 0) {
			num_vertices = nv;
			info = new vertex_info[num_vertices];
			seen_vertices.resize(num_vertices);
		} else if (num_vertices != nv)
			throw std::runtime_error(
					"Error while processing " + filename + ": wrong number of vertices");
		fseeko(count_files[i], 0L, SEEK_END);
		off_t size = ftello(count_files[i]);
		assert(size >= 0);
		assert(
				static_cast<std::make_unsigned<off_t>::type>(size)
						<= std::numeric_limits<size_t>::max());
		cnt_map[i].second = static_cast<size_t>(size);
		cnt_map[i].first = static_cast<char*>(motivo_mmap_populate(cnt_map[i].second, PROT_READ,
				fileno(count_files[i])));
		if (cnt_map[i].first == MAP_FAILED)
			throw std::runtime_error("Error while processing " + filename + ": cannot mmap file");
		const char* end = cnt_map[i].first + cnt_map[i].second;
		char* ptr = cnt_map[i].first + sizeof(UndirectedGraph::vertex_t);
		while (ptr + sizeof(UndirectedGraph::vertex_t) + sizeof(uint64_t) <= end) {
			UndirectedGraph::vertex_t vertex;
			memcpy(&vertex, ptr, sizeof(UndirectedGraph::vertex_t));
			ptr += sizeof(UndirectedGraph::vertex_t);
			uint64_t number_of_occurrences;
			memcpy(&number_of_occurrences, ptr, sizeof(uint64_t));
			ptr += sizeof(uint64_t);
			assert(vertex < num_vertices);
			if (seen_vertices[vertex])
				throw std::runtime_error(
						"Error while processing " + filename + ": duplicate vertex "
								+ std::to_string(vertex));
			seen_vertices[vertex] = true;
			info[vertex].ptr = ptr;
			info[vertex].count = number_of_occurrences;
			ptr += number_of_occurrences * sizeof(TreeletTable::treelet_count_pair);
		}

		if (ptr != end)
			throw std::runtime_error(
					"Error while processing " + filename + ": abnormal file termination");
	}

	write_table(output_basename, num_vertices, info);

	delete[] info;
	for (unsigned int i = 0; i < no_files; i++) {
		motivo_munmap(cnt_map[i].first, cnt_map[i].second);
		fclose(count_files[i]);
	}
	delete[] cnt_map;
	delete[] count_files;
}

/**
 * Compute the spanning tree table of a graphlet
 */
CachedSTC::treelet_table_t* CachedSTC::compute_t_table(const Occurrence &o) {
	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
	CachedSTC::treelet_table_t* tab = new CachedSTC::treelet_table_t();
//	tab->set_empty_key(Treelet::invalid_treelet);

	UndirectedGraph h(o);
	FullGraphColoring *coloring = new FullGraphColoring();
	TreeletTableCollection ttc;
	TreeletTable** tables = nullptr;

	// the table for 1-graphlets
	const std::string filename = "spantreecount.1.cnt";
	std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
	if (out.bad())
		throw std::runtime_error("Could not open output file for writing");
	SimpleTreeletTableBuilder builder(&h, coloring, 1, &ttc, &out, false, nullptr);
	builder.build();
	out.close();
	std::vector<std::string> vf;
	vf.push_back(filename);
	merge(vf, "spantreecount.1");
	CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias>* readers = nullptr;
	readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias> [h.number_of_vertices() - 1];
	tables = new TreeletTable*[h.number_of_vertices() - 1];
	for (unsigned int i = 2; i <= h.number_of_vertices(); i++) {
		readers[i - 2].open("spantreecount." + std::to_string(i - 1) + ".dtz");
		readers[i - 2].prefault(0, h.number_of_vertices() - 1);
		tables[i - 2] = new TreeletTable(&readers[i - 2]);
		ttc.add(tables[i - 2]);
		const std::string filename = "spantreecount." + std::to_string(i) + ".cnt";
		std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
		SimpleTreeletTableBuilder builder(&h, coloring, i, &ttc, &out, false, nullptr);
		builder.build();
		out.close();
		std::vector<std::string> vf;
		vf.push_back(filename);
		merge(vf, "spantreecount." + std::to_string(i));
	}
	delete[] readers;
	delete[] tables;
	CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias> reader;
	reader.open("spantreecount." + std::to_string(h.number_of_vertices()) + ".dtz");
	reader.prefault(0, h.number_of_vertices() - 1);
	TreeletTable finalTable(&reader);
	for (UndirectedGraph::vertex_t u = 0; u < finalTable.number_of_vertices(); u++)
		for (TreeletTable::const_iterator it = finalTable.begin(u); !it.is_over(); ++it)
			(*tab)[it.treelet()] += it.count();
	reader.close();
	std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
	tot_computing_time += delta_t.count();
	return tab;
}

/**
 * Return the number of occurrences of t in o
 */
uint64_t CachedSTC::num_spanning_trees(const Occurrence &o, const Treelet &t) {
	if (table.count(o) == 0) {
		m_mutex.lock();
		table[o] = compute_t_table(o);
		m_mutex.unlock();
	}
	return table[o]->count(t) ? (*table[o])[t] : 0;
}

