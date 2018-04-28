/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2016  James Lovejoy

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

#include <sstream>

#include <json/writer.h>
#include <json/reader.h>

#include <leveldb/write_batch.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>

#include "storage.h"

CryptoKernel::Storage::Storage(const std::string& filename, const bool sync, const unsigned int cache, const bool bloom) {
    leveldb::Options options;
    options.create_if_missing = true;

    if(cache > 0) {
        options.block_cache = leveldb::NewLRUCache(cache * 1024 * 1024);
    }

    if(bloom) {
        options.filter_policy = leveldb::NewBloomFilterPolicy(8 * 2);
    }

    this->sync = sync;

    dbMutex.lock();
    leveldb::Status dbstatus = leveldb::DB::Open(options, filename, &db);
    dbMutex.unlock();

    if(!dbstatus.ok()) {
        throw std::runtime_error("Failed to open the database");
    }
}

CryptoKernel::Storage::~Storage() {
    dbMutex.lock();
    delete db;
    dbMutex.unlock();
}

Json::Value CryptoKernel::Storage::toJson(const std::string& json) {
    Json::Value returning;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::stringstream input;
    input << json;
    try {
        Json::parseFromStream(rbuilder, input, &returning, &errs);
    } catch(const Json::Exception& e) {
        return Json::Value();
    }

    return returning;
}

std::string CryptoKernel::Storage::toString(const Json::Value& json, const bool pretty) {
    if(pretty) {
        Json::StyledWriter writer;
        return writer.write(json);
    } else {
        Json::FastWriter writer;
        return writer.write(json);
    }
}

bool CryptoKernel::Storage::destroy(const std::string& filename) {
    leveldb::Options options;
    leveldb::DestroyDB(filename, options);

    return true;
}

CryptoKernel::Storage::Transaction* CryptoKernel::Storage::begin() {
    return new Transaction(this);
}

CryptoKernel::Storage::Transaction* CryptoKernel::Storage::begin(
    std::recursive_mutex& mut) {
    return new Transaction(this, mut);
}

CryptoKernel::Storage::Transaction::Transaction(CryptoKernel::Storage* db) {
    db->dbMutex.lock();
    this->db = db;
    mut = nullptr;
    finished = false;
}

CryptoKernel::Storage::Transaction::Transaction(CryptoKernel::Storage* db,
        std::recursive_mutex& mut) {
    db->dbMutex.lock();
    this->db = db;
    this->mut = &mut;
    finished = false;
}

CryptoKernel::Storage::Transaction::~Transaction() {
    if(!finished) {
        abort();
    }

    if(mut != nullptr) {
        mut->unlock();
    }
}

bool CryptoKernel::Storage::Transaction::ended() {
    return finished;
}

void CryptoKernel::Storage::Transaction::commit() {
    if(!finished) {
        leveldb::WriteBatch batch;
        for(auto& update : dbStateCache) {
            if(update.second.erased) {
                batch.Delete(update.first);
            } else {
                batch.Put(update.first, CryptoKernel::Storage::toString(update.second.data));
            }
        }

        leveldb::WriteOptions options;
        options.sync = db->sync;

        leveldb::Status status = db->db->Write(options, &batch);

        if(!status.ok()) {
            throw std::runtime_error("Could not commit transaction " + status.ToString());
        }

        abort();
    } else {
        throw std::runtime_error("Attempted to commit finished transaction");
    }
}

void CryptoKernel::Storage::Transaction::abort() {
    finished = true;
    db->dbMutex.unlock();
}

void CryptoKernel::Storage::Transaction::put(const std::string& key,
        const Json::Value& data) {
    dbStateCache[key] = dbObject{data, false};
}

void CryptoKernel::Storage::Transaction::erase(const std::string& key) {
    dbStateCache[key] = dbObject{Json::Value(), true};
}

Json::Value CryptoKernel::Storage::Transaction::get(const std::string& key) {
    const auto it = dbStateCache.find(key);
    if(it != dbStateCache.end()) {
        return it->second.data;
    } else {
        std::string data;
        db->db->Get(leveldb::ReadOptions(), key, &data);
        return CryptoKernel::Storage::toJson(data);
    }
}

CryptoKernel::Storage::Table::Table(const std::string& name) {
    tableName = name;
}

std::string CryptoKernel::Storage::Table::getKey(const std::string& key,
        const int index) {
    return tableName + "/" + std::to_string(index + 1) + "/" + key;
}

void CryptoKernel::Storage::Table::put(Transaction* transaction, const std::string& key,
                                       const Json::Value& data, const int index) {
    transaction->put(getKey(key, index), data);
}

void CryptoKernel::Storage::Table::erase(Transaction* transaction, const std::string& key,
        const int index) {
    transaction->erase(getKey(key, index));
}

Json::Value CryptoKernel::Storage::Table::get(Transaction* transaction,
        const std::string& key, const int index) {
    return transaction->get(getKey(key, index));
}

CryptoKernel::Storage::Table::Iterator::Iterator(Table* table, Storage* db) {
    this->table = table;
    this->db = db;
    db->dbMutex.lock();

    it = db->db->NewIterator(leveldb::ReadOptions());

    prefix = table->getKey("");
}

CryptoKernel::Storage::Table::Iterator::~Iterator() {
    delete it;
    db->dbMutex.unlock();
}

void CryptoKernel::Storage::Table::Iterator::SeekToFirst() {
    it->Seek(prefix);
}

bool CryptoKernel::Storage::Table::Iterator::Valid() {
    if(it->Valid()) {
        return it->key().ToString().compare(0, prefix.size(), prefix) == 0;
    } else {
        return false;
    }
}

void CryptoKernel::Storage::Table::Iterator::Next() {
    it->Next();
}

std::string CryptoKernel::Storage::Table::Iterator::key() {
    return it->key().ToString().substr(prefix.size());
}

Json::Value CryptoKernel::Storage::Table::Iterator::value() {
    return CryptoKernel::Storage::toJson(it->value().ToString());
}
