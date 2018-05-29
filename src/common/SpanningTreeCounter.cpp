/*
 * SpanningTreeCounter.cpp
 *
 *  Created on: 15 mag 2018
 *      Author: brix
 */

#include "SpanningTreeCounter.h"
#include "graph/FullGraphColoring.h"
#include "treelets/TreeletTable.h"
#include "treelets/TreeletTableCollection.h"
#include "../builder/SimpleTreeletTableBuilder.h"
#include "../builder/TreeletTableBuilder.h"
#include <string>

unsigned int bits_needed(uint128_t n) {
	unsigned int needed = 1;
	for (n >>= 1; n != 0; n >>= 1)
		needed++;

	return needed;
}

std::string to_string(uint128_t n) {
	static const constexpr uint128_t ten_19 = 0x8ac7230489e80000; //10^19;
	static const constexpr uint128_t ten_38 = ten_19 * ten_19; //Maximum power of 10 representable with an uint128_t

	if (n == 0)
		return "0";

	std::string s = "";
	bool significant_digit_found = false;
	for (uint128_t max_dec = ten_38; max_dec != 0; max_dec /= 10) {
		unsigned int digit = static_cast<unsigned int>(n / max_dec);
		n = n % max_dec;
		assert(digit <= 9);
		if (significant_digit_found || digit != 0) {
			significant_digit_found = true;
			s += static_cast<char>('0' + digit);
		}
	}

	return s;
}

struct vertex_info {
	char* ptr;
	uint64_t count = 0;
};

void write_table(const std::string &output_basename, const UndirectedGraph::vertex_t num_vertices,
		vertex_info* info, double compression_threshold) {
	uint64_t num_treelet_count_pairs = 0;
	TreeletTable::treelet_count_t num_occ_treelet = 0;
	uint128_t num_occ_total = 0;
	uint128_t num_occ_max = 0;
	bool num_occ_total_overflow = false;

	std::string output_filename = output_basename + ".dtz";
	CompressedRecordFileWriter writer(output_filename, num_vertices);

	AliasMethodSampler<UndirectedGraph::vertex_t, TreeletTable::treelet_count_t> alias_sampler(
			num_vertices);

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
				(info[u].count + 1) * sizeof(TreeletTable::treelet_count_pair),
				compression_threshold);

		if (add_overflow(num_occ_total, p->count, &num_occ_total))
			num_occ_total_overflow = true;

		if (p->count > num_occ_max)
			num_occ_max = p->count;

		alias_sampler.set(u, p->count);
		delete[] to_write;
	}

	writer.close();

	/*
	 std::cout << "Compressed size: " << writer.get_compressed_size() << " Original size: "
	 << writer.get_uncompressed_size() << " Ratio: "
	 << static_cast<double>(writer.get_compressed_size())
	 / static_cast<double>(writer.get_uncompressed_size()) << std::endl;
	 */

//	std::cout << "Building root sampler alias table... ";
	std::string root_sampler_filename = output_basename + ".rts";
	alias_sampler.build();
	alias_sampler.write(root_sampler_filename);
//	std::cout << "done" << std::endl;

	/*
	 std::cout << "Processed " << num_vertices << " vertices (wrote " << num_treelet_count_pairs
	 << " counts)" << std::endl;
	 std::cout << "Total number of treelet occurrences: ";
	 if (num_occ_total_overflow)
	 std::cout << "Overflow!" << std::endl;
	 else
	 std::cout << to_string(num_occ_total) << " (" << bits_needed(num_occ_total) << " bits)"
	 << std::endl;
	 std::cout << "Maximum number of occurrences rooted in a single vertex: "
	 << to_string(num_occ_max) << " (" << bits_needed(num_occ_max) << " bits)" << std::endl;
	 std::cout << "Maximum number of occurrences of a single rooted treelet: "
	 << to_string(num_occ_treelet) << " (" << bits_needed(num_occ_treelet) << " bits)"
	 << std::endl;
	 std::cout << "Output written to files: " << output_filename << ", and " << root_sampler_filename
	 << std::endl;
	 */
}

/**
 * Copied from merger.cpp
 */
void merge(const std::vector<std::string>& count_filenames, const std::string& output_basename,
		double compression_threshold) {
	const unsigned long no_files = count_filenames.size();
	UndirectedGraph::vertex_t num_vertices = 0;
	std::pair<char*, size_t>* cnt_map = new std::pair<char*, size_t>[no_files];
	FILE** count_files = new FILE*[no_files];
	vertex_info* info = nullptr;
	std::vector<bool> seen_vertices;
//	std::cout << "merge: before loop" << std::endl;

	for (unsigned int i = 0; i < no_files; i++) {
//		std::cout << "merge: loop " << i << std::endl;
		const std::string &filename = count_filenames[i];
		count_files[i] = fopen(filename.c_str(), "rb");

		if (count_files[i] == NULL)
			throw std::runtime_error("Unable to open file " + filename);

		UndirectedGraph::vertex_t nv;
		fread(&nv, sizeof(UndirectedGraph::vertex_t), 1, count_files[i]);

//		std::cout << "merge: loop " << i << std::endl;
//		std::cout << "merge: nv=" << nv << std::endl;

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

	write_table(output_basename, num_vertices, info, compression_threshold);

	delete[] info;

	for (unsigned int i = 0; i < no_files; i++) {
		motivo_munmap(cnt_map[i].first, cnt_map[i].second);
		fclose(count_files[i]);
	}

	delete[] cnt_map;
	delete[] count_files;
}

SpanningTreeCounter::SpanningTreeCounter() {
	// TODO Auto-generated constructor stub
}

SpanningTreeCounter::~SpanningTreeCounter() {
	// TODO Auto-generated destructor stub
}

/**
 * Compute the number of spanning trees of the occurrence
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ) {
	uint64_t c = 0;
	return occ.number_of_spanning_trees();
}

/**
 * Compute the number of spanning trees of the occurrence, possibly including/excluding some
 */
uint64_t SpanningTreeCounter::num_spanning_trees(const Occurrence& occ, TreeletSelector* ts) {
	UndirectedGraph h(occ);
	FullGraphColoring *coloring = new FullGraphColoring();
	TreeletTableCollection ttc;
	TreeletTable** tables = nullptr;

	// the table for 1-graphlets
	const std::string filename = "spantreecount.1.cnt";
	std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
	if (out.bad())
		throw std::runtime_error("Could not open output file for writing");
	SimpleTreeletTableBuilder builder(&h, coloring, 1, &ttc, &out, false, ts);
	builder.build();
	out.close();
//	std::cout << "invoking merge..." << std::endl;
	std::vector<std::string> vf;
	vf.push_back(filename);
	merge(vf, "spantreecount.1", 0);

//	std::chrono::time_point < std::chrono::steady_clock > tstart = std::chrono::steady_clock::now();
	builder.build();
//	std::cout << "BUILT TABLE 1" << std::endl;
//	std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
	CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias>* readers = nullptr;
	readers = new CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias> [h.number_of_vertices() - 1];
	tables = new TreeletTable*[h.number_of_vertices() - 1];
	for (unsigned int i = 2; i <= h.number_of_vertices(); i++) {
		// build tables of size i from those of size 1,...,i-1
//		std::cout << "BUILDING TABLE " << i << std::endl;
		readers[i - 2].open("spantreecount." + std::to_string(i - 1) + ".dtz");
		readers[i - 2].prefault(0, h.number_of_vertices() - 1);
//		std::cout << "files opened " << std::endl;
		tables[i - 2] = new TreeletTable(&readers[i - 2]);
		ttc.add(tables[i - 2]);
//		std::cout << "num vertices in last table = " << ttc.get_table(1)->number_of_vertices()
//				<< std::endl;
		const std::string filename = "spantreecount." + std::to_string(i) + ".cnt";
		std::ofstream out(filename, std::ofstream::binary | std::ofstream::trunc);
		SimpleTreeletTableBuilder builder(&h, coloring, i, &ttc, &out, i == h.number_of_vertices(),
				ts);
//        std::chrono::time_point<std::chrono::steady_clock>  tstart = std::chrono::steady_clock::now();
		builder.build();
//        std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - tstart;
//        std::cerr << "Building time: " << delta_t.count() << " s\n";
		out.close();
//		std::cout << "BUILT TABLE " << i << std::endl;
//        std::cout << "Output written to " << filename << std::endl;
//		std::cout << "invoking merge..." << std::endl;
		std::vector<std::string> vf;
		vf.push_back(filename);
		merge(vf, "spantreecount." + std::to_string(i), 0);
	}
	CompressedRecordFileReader<const TreeletTable::treelet_count_pair_maybe_alias,
			TreeletTable::may_alias> reader;
	reader.open("spantreecount." + std::to_string(h.number_of_vertices()) + ".dtz");
	reader.prefault(0, h.number_of_vertices() - 1);
	TreeletTable finalTable(&reader);
	uint64_t cnt = 0;
	for (UndirectedGraph::vertex_t u = 0; u < h.number_of_vertices(); u++)
		for (TreeletTable::const_iterator u_it = finalTable.begin(u); !u_it.is_over(); ++u_it) {
//			std::cout << static_cast<uint64_t>(u_it.count()) << std::endl;
			cnt += u_it.count();
		}
	return cnt;

}

/**
 * Return the number of spanning stars
 */
uint64_t SpanningTreeCounter::num_spanning_stars(const Occurrence& occ) {
	UndirectedGraph h(occ);
	int count = 0;
	for (UndirectedGraph::vertex_t v = 0; v < h.number_of_vertices(); v++)
		count += h.degree(v) == h.number_of_vertices() - 1 ? 1 : 0;
	return count;
}
