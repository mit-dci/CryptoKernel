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
#include <memory>

#include <json/writer.h>
#include <json/reader.h>

#include <leveldb/write_batch.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>

#include "storage.h"

CryptoKernel::Storage::Storage(const std::string& filename, const bool sync, const unsigned int cache, const bool bloom) {
    options.create_if_missing = true;

    if(cache > 0) {
        options.block_cache = leveldb::NewLRUCache(cache * 1024 * 1024);
    }

    if(bloom) {
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    }

    this->sync = sync;

    writeLock.lock();
    readLock.lock();
    leveldb::Status dbstatus = leveldb::DB::Open(options, filename, &db);
    readLock.unlock();
    writeLock.unlock();

    if(!dbstatus.ok()) {
        throw std::runtime_error("Failed to open the database");
    }
}

CryptoKernel::Storage::~Storage() {
    writeLock.lock();
    readLock.lock();
    delete db;
    delete options.block_cache;
    delete options.filter_policy;
    readLock.unlock();
    writeLock.unlock();
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
    std::stringstream buf;   
    Json::StreamWriterBuilder builder;
    std::unique_ptr<Json::StreamWriter> writer;     
    if(!pretty) {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
    }
    writer.reset(builder.newStreamWriter());
    writer->write(json, &buf);
    return buf.str() + "\n";
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

CryptoKernel::Storage::Transaction* CryptoKernel::Storage::beginReadOnly() {
    return new Transaction(this, true);
}

CryptoKernel::Storage::Transaction::Transaction(CryptoKernel::Storage* db,
                                                const bool readonly) {
    if(!readonly) {
        db->writeLock.lock();
        finished = false;
    } else {
        std::lock_guard<std::mutex> lock(db->readLock);
        snapshot = db->db->GetSnapshot();
        finished = true;
    }
    this->db = db;
    this->readonly = readonly;
    mut = nullptr;
}

CryptoKernel::Storage::Transaction::Transaction(CryptoKernel::Storage* db,
                                                std::recursive_mutex& mut,
                                                const bool readonly) {
    if(!readonly) {
        db->writeLock.lock();
        finished = false;
    } else {
        std::lock_guard<std::mutex> lock(db->readLock);
        snapshot = db->db->GetSnapshot();
        finished = true;
    }
    this->db = db;
    this->mut = &mut;
    this->readonly = readonly;
}

CryptoKernel::Storage::Transaction::~Transaction() {
    if(!finished) {
        abort();
    }

    if(readonly) {
        std::lock_guard<std::mutex> lock(db->readLock);
        db->db->ReleaseSnapshot(snapshot);
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

        std::lock_guard<std::mutex> lock(db->readLock);
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
    if(!readonly) {
        db->writeLock.unlock();
    }
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
        
        leveldb::ReadOptions options;
        if(readonly) {
            options.snapshot = snapshot;
        }

        db->db->Get(options, key, &data);
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

CryptoKernel::Storage::Table::Iterator::Iterator(Table* table, Storage* db, const 
leveldb::Snapshot* snapshot, const std::string& prefix, const int index) {
    this->table = table;
    this->db = db;
    
    leveldb::ReadOptions options;

    this->snapshot = snapshot;

    if(snapshot != nullptr) {
        options.snapshot = snapshot;
    } else {
         db->writeLock.lock();
    }

    it = db->db->NewIterator(options);

    this->prefix = table->getKey("", index);

    if(prefix != "") {
        this->prefix += prefix;
    }
}

CryptoKernel::Storage::Table::Iterator::~Iterator() {
    delete it;
    if(snapshot == nullptr) {
        db->writeLock.unlock();
    }
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
