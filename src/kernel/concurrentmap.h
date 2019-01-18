/*
 * ConcurrentMap.h
 *
 *  Created on: Jul 11, 2018
 *      Author: Luke Horgan
 */

#ifndef CONCMAP_H_
#define CONCMAP_H_

#include <map>
#include <vector>
#include <mutex>

template <class KEY, class VAL> class ConcurrentMap {
public:
	ConcurrentMap() {};

	std::size_t size() {
		mapMutex.lock();
		size_t size = map.size();
		mapMutex.unlock();
		return size;
	}

	typename std::map<KEY, VAL>::iterator find(KEY key) {
		mapMutex.lock();
		auto res = map.find(key);
		mapMutex.unlock();
		return res;
	}

	VAL& at(KEY key) {
		mapMutex.lock();
		VAL& res = map[key];
		mapMutex.unlock();
		return res;
	}

	bool contains(KEY key) {
		bool found = false;
		mapMutex.lock();
		if(map.find(key) != map.end()) {
			found = true;
		}
		mapMutex.unlock();
		return found;
	}

	typename std::map<KEY, VAL>::iterator begin() {
		mapMutex.lock();
		auto res = map.begin();
		mapMutex.unlock();
		return res;
	}

	typename std::map<KEY, VAL>::iterator end() {
		mapMutex.lock();
		auto res = map.end();
		mapMutex.unlock();
		return res;
	}

	typename std::vector<KEY> keys() {
		mapMutex.lock();
		std::vector<KEY> keys;
		for(auto it = map.begin(); it != map.end(); it++) {
			keys.push_back(it->first);
		}
		mapMutex.unlock();
		return keys;
	}

	void erase(KEY key) {
		mapMutex.lock();
		map.erase(key);
		mapMutex.unlock();
	}

	void erase(typename std::map<KEY, VAL>::iterator it) {
		mapMutex.lock();
		map.erase(it);
		mapMutex.unlock();
	}

	void clear() {
		mapMutex.lock();
		map.clear();
		mapMutex.unlock();
	}

	void insert(KEY key, VAL val) {
		mapMutex.lock();
		map[key] = val;
		mapMutex.unlock();
	}

	void insert(std::pair<KEY, VAL> pair) {
		mapMutex.lock();
		map[pair.first] = pair.second;
		mapMutex.unlock();
	}

	std::map<KEY, VAL> copyMap() {
		std::map<KEY, VAL> mapCopy;
		mapMutex.lock();
		for(auto it = map.begin(); it != map.end(); it++) {
			mapCopy[it->first] = it->second;
		}
		mapMutex.unlock();
		return mapCopy;
	}

	virtual ~ConcurrentMap() {};

private:
	std::map<KEY, VAL> map;
	std::mutex mapMutex; // someday, this should be a shared mutex
};

#endif /* CONCMAP_H_ */
