// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SRC_COMMON_VALUESORTEDMAP_H_
#define SRC_COMMON_VALUESORTEDMAP_H_

#include <map>

/**
 * A map that keeps the elements sorted in nondecreasing order of value V.
 */
template<typename K, typename V> class ValueSortedMap
{

private:
	struct pairCompare
	{
		inline bool operator()(const std::pair<K, V> &p1, const std::pair<K, V> &p2) const
		{
			return (p1.second < p2.second) || ((p1.second <= p2.second) && (p1.first < p2.first)); //Use <= instead of = to silence warning
		}
	};

	std::map<K, V> m1;
	std::map<std::pair<K, V>, bool, pairCompare> m2;

public:
	inline uint64_t size()
	{
		return m2.size();
	}

	inline void erase(const K &key) {
		if (m1.count(key)) {
			m2.erase(std::pair<K, V>(key, m1[key]));
			m1.erase(key);
		}
	}

	inline void insert(const K &key, const V &value) {
		erase(key);
		m1[key] = value;
		m2[std::pair<K, V>(key, value)] = true;
	}

	inline typename std::map<K, V>::iterator begin() {
		return m2.begin();
	}

	inline typename std::map<K, V>::iterator end() {
		return m2.end();
	}

	inline typename std::map<K, V>::reverse_iterator rbegin() {
		return m2.rbegin();
	}

	inline typename std::map<K, V>::reverse_iterator rend() {
		return m2.rend();
	}

	inline K first_key() {
		return m2.begin()->first.first;
	}

	inline V first_value() {
		return m2.begin()->first.second;
	}

	inline K last_key() {
		return m2.rbegin()->first.first;
	}

	inline V last_value() {
		return m2.rbegin()->first.second;
	}

	inline V& get(const K& key) {
		return m1[key];
	}

	friend std::ostream& operator<<(std::ostream& os, const ValueSortedMap<K, V>& st)
    {
		for (const auto &it : st.m2)
			os << it.first.first.get_structure() << "," << it.first.second << "\n";

		return os;
	}

};

#endif /* SRC_COMMON_VALUESORTEDMAP_H_ */
