/*
 * ConcMap.h
 *
 *  Created on: Jul 11, 2018
 *      Author: Luke Horgan
 */

#ifndef CONCMAP_H_
#define CONCMAP_H_

#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>

namespace CryptoKernel {
template <class KEY, class VAL> class ConcurrentMap {
public:
	ConcurrentMap() {
	}

	std::size_t size() {
		mapMutex.lock_shared();
		size_t size = map.size();
		mapMutex.unlock_shared();
		return size;
	}

	typename std::map<KEY, VAL>::iterator find(KEY key) {
		mapMutex.lock_shared();
		auto res = map.find(key);
		mapMutex.unlock_shared();
		return res;
	}

	VAL& at(KEY key) {
		return map[key];
	}

	bool contains(KEY key) {
		bool found = false;
		mapMutex.lock_shared();
		if(map.find(key) != map.end()) {
			found = true;
		}
		mapMutex.unlock_shared();
		return found;
	}

	typename std::map<KEY, VAL>::iterator begin() {
		mapMutex.lock_shared();
		auto res = map.begin();
		mapMutex.unlock_shared();
		return res;
	}

	typename std::map<KEY, VAL>::iterator end() {
		mapMutex.lock_shared();
		auto res = map.end();
		mapMutex.unlock_shared();
		return res;
	}

	typename std::vector<KEY> keys() {
		mapMutex.lock_shared();
		std::vector<KEY> keys;
		for(auto it = map.begin(); it != map.end(); it++) {
			keys.push_back(it->first);
		}
		mapMutex.unlock_shared();
		return keys;
	}

	void erase(typename std::map<KEY, VAL>::iterator it) {
		mapMutex.lock();
		map.erase(it);
		mapMutex.unlock();
	}

	void insert(KEY key, VAL val) {
		mapMutex.lock();
		map[key] = val;
		mapMutex.unlock();
	}

	virtual ~ConcurrentMap() {};

private:
	std::map<KEY, VAL> map;
	std::shared_timed_mutex mapMutex;
};
}

#endif /* CONCMAP_H_ */
