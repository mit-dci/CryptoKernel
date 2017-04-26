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

#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/reader.h>

#include <leveldb/write_batch.h>

#include "storage.h"

CryptoKernel::Storage::Storage(const std::string filename)
{
    leveldb::Options options;
    options.create_if_missing = true;

    dbMutex.lock();
    leveldb::Status dbstatus = leveldb::DB::Open(options, filename, &db);
    dbMutex.unlock();

    if(!dbstatus.ok())
    {
        throw std::runtime_error("Failed to open the database");
    }
}

CryptoKernel::Storage::~Storage()
{
    dbMutex.lock();
    delete db;
    dbMutex.unlock();
}

Json::Value CryptoKernel::Storage::toJson(std::string json)
{
    Json::Value returning;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::stringstream input;
    input << json;
    Json::parseFromStream(rbuilder, input, &returning, &errs);

    return returning;
}

std::string CryptoKernel::Storage::toString(Json::Value json, bool pretty)
{
    if(pretty)
    {
        Json::StyledWriter writer;
        return writer.write(json);
    }
    else
    {
        Json::FastWriter writer;
        return writer.write(json);
    }
}

bool CryptoKernel::Storage::destroy(std::string filename)
{
    leveldb::Options options;
    leveldb::DestroyDB(filename, options);

    return true;
}

CryptoKernel::Storage::Transaction* CryptoKernel::Storage::begin() {
    return new Transaction(this);
}

CryptoKernel::Storage::Transaction::Transaction(CryptoKernel::Storage* db) {
    db->dbMutex.lock();
    this->db = db;
    finished = false;
}

CryptoKernel::Storage::Transaction::~Transaction() {
    if(!finished) {
        abort();
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
        options.sync = true;

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

void CryptoKernel::Storage::Transaction::put(const std::string key, const Json::Value data) {
    dbStateCache[key] = dbObject{data, false};
}

void CryptoKernel::Storage::Transaction::erase(const std::string key) {
    dbStateCache[key] = dbObject{Json::Value(), true};
}

Json::Value CryptoKernel::Storage::Transaction::get(const std::string key) {
    const auto it = dbStateCache.find(key);
    if(it != dbStateCache.end()) {
        return it->second.data;
    } else {
        std::string data;
        db->db->Get(leveldb::ReadOptions(), key, &data);
        return CryptoKernel::Storage::toJson(data);
    }
}

CryptoKernel::Storage::Table::Table(const std::string name) {
    tableName = name;
}

std::string CryptoKernel::Storage::Table::getKey(const std::string key, const int index) {
    return tableName + "/" + std::to_string(index + 1) + "/" + key;
}

void CryptoKernel::Storage::Table::put(Transaction* transaction, const std::string key, const Json::Value data, const int index) {
    transaction->put(getKey("", index), Json::Value());
    transaction->put(getKey(key, index), data);
}

void CryptoKernel::Storage::Table::erase(Transaction* transaction, const std::string key, const int index) {
    transaction->erase(getKey(key, index));
}

Json::Value CryptoKernel::Storage::Table::get(Transaction* transaction, const std::string key, const int index) {
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
    it->Next();
}

bool CryptoKernel::Storage::Table::Iterator::Valid() {
    if(it->Valid()) {
        return it->key().ToString().compare(0, prefix.size(), prefix);
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
