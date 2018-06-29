/*
 * ValueSortedMap.h
 *
 *  Created on: 28 giu 2018
 *      Author: brix
 */

#ifndef SRC_COMMON_VALUESORTEDMAP_H_
#define SRC_COMMON_VALUESORTEDMAP_H_

#include <map>

template<typename K, typename V> class ValueSortedMap {
private:
	struct pairCompare {
		inline bool operator()(const std::pair<K, V> &p1, const std::pair<K, V> &p2) {
			return (p1.second < p2.second) || ((p1.second == p2.second) && (p1.first < p2.first));
		}
	};
	std::map<K, V> m1;
	std::map<std::pair<K, V>, bool, pairCompare> m2;
public:
	ValueSortedMap<K, V>() {
	}

	~ValueSortedMap<K, V>() {
	}

	inline int size() {
		return m2.size();
	}

	inline bool empty() {
		return m1.empty();
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

	friend std::ostream& operator<<(std::ostream& os, const ValueSortedMap<K, V>& st) {
		for (auto it : st.m2) {
			os << it.first.first.get_structure();
			os << "," << it.first.second;
			os << std::endl;
		}
		return os;
	}

};

#endif /* SRC_COMMON_VALUESORTEDMAP_H_ */
