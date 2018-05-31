//
// Created by steven on 3/20/18.
//

#ifndef MOTIVO_TREELETSELECTOR_H
#define MOTIVO_TREELETSELECTOR_H

#include <cstdint>
#include <algorithm>
#include "Treelet.h"
#include <fstream>

class TreeletSelector {
public:
	typedef int mode_t;
	constexpr static int MODE_INCLUDE = 1;
	constexpr static int MODE_EXCLUDE = 2;

private:
	uint64_t size = 0;
	uint64_t capacity = 0;
	Treelet* treelets = nullptr;
	mode_t mode;

	const unsigned int treelet_size; //0 for any size. >0 to restrict to the given size

public:
	TreeletSelector(const mode_t mode, const unsigned int treelet_size = 0) :
			mode(mode), treelet_size(treelet_size) {
	}

	TreeletSelector(const std::string& filename, const unsigned int treelet_size = 0) :
			treelet_size(treelet_size) {

		std::ifstream ifs(filename, std::ifstream::binary);
		if (!ifs.is_open())
			throw std::runtime_error("Could not open file " + filename);

		std::string mode_string;
		ifs >> mode_string;
		if (mode_string == "INCLUDE")
			mode = MODE_INCLUDE;
		else if (mode_string == "EXCLUDE")
			mode = MODE_EXCLUDE;
		else
			throw std::runtime_error("Invalid mode in " + filename);

		Treelet::treelet_structure_t structure;
		Treelet::treelet_colors_t colors;
		while (ifs >> structure >> colors)
			add_treelet(Treelet(structure, colors));

		std::sort(treelets, treelets + size);

		for (unsigned int i = 1; i < size; i++) {
			if ((treelets[i - 1] == treelets[i])
					|| (!treelets[i - 1].is_colored()
							&& treelets[i - 1].get_structure() == treelets[i].get_structure()))
				throw std::runtime_error("Duplicate or redudant treelet selection pattern");
		}
	}

	~TreeletSelector() {
		delete[] treelets;
	}

	uint64_t get_size() {
		return size;
	}

	mode_t get_mode() {
		return mode;
	}

	void add_treelet(Treelet treelet, bool sort_treelets = false) {
		if (treelet_size != 0 && treelet.number_of_vertices() != treelet_size)
			return;

		if (size == capacity) {
			capacity = (capacity == 0) ? 2 : (capacity * 2);
			Treelet *t = new Treelet[capacity];

			std::copy(treelets, treelets + size, t);
			delete[] treelets;
			treelets = t;
		}

		treelets[size++] = treelet;
		if (sort_treelets)
			std::sort(treelets, treelets + size);
	}

	const Treelet* get_treelets() {
		return treelets;
	}
	;

	unsigned int get_treelet_size() const {
		return treelet_size;
	}
	;

	bool is_included(Treelet t) const {
		//FIXME: Binary search? Stop early?
		bool found = false;
		for (uint64_t i = 0; i < size; i++) {
			if (treelets[i].get_structure() == t.get_structure()
					&& (!treelets[i].is_colored() || treelets[i].get_colors() == t.get_colors())) {
				found = true;
				break;
			}
		}

		return (mode == MODE_INCLUDE) == found;
	}

	/**
	 * A selector that includes only the stars on k nodes
	 */
	static TreeletSelector get_star_includer(int k) {
		TreeletSelector ts(TreeletSelector::MODE_INCLUDE, k);
		Treelet::treelet_structure_t s = 0;
		// the k-star rooted at the center
		for (int i = 0; i < k - 1; i++)
			s |= Treelet::treelet_structure_highest_bit >> i * 2;
		ts.add_treelet(Treelet(s, 0), true);
		if (k > 2) {
			// the k-star rooted at one leaf
			s <<= 1;
			s |= Treelet::treelet_structure_highest_bit;
			ts.add_treelet(Treelet(s, 0), true);
		}
		return ts;
	}

	/**
	 * A selector that excludes only the stars on k nodes
	 */
	static TreeletSelector get_star_excluder(int k) {
		TreeletSelector ts = get_star_includer(k);
		ts.mode = TreeletSelector::MODE_EXCLUDE;
		return ts;
	}

};

#endif //MOTIVO_TREELETSELECTOR_H
