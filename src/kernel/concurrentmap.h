/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2019  Luke Horgan, James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

	std::pair<bool, VAL> atMaybe(KEY key) {
		std::pair<bool, VAL> returning;
		mapMutex.lock();
		auto res = map.find(key);
		if(res != map.end()) {
			returning = std::make_pair(true, res->second);
		} else {
			returning = std::make_pair(false, nullptr);
		}
		mapMutex.unlock();

		return returning;
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
