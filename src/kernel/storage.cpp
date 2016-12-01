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

#include "storage.h"

CryptoKernel::Storage::Storage(std::string filename)
{
    leveldb::Options options;
    options.create_if_missing = true;

    dbMutex.lock();
    leveldb::Status dbstatus = leveldb::DB::Open(options, filename, &db);
    dbMutex.unlock();

    if(!dbstatus.ok())
    {
        status = false;
    }
    else
    {
        status = true;
    }
}

CryptoKernel::Storage::~Storage()
{
    dbMutex.lock();
    delete db;
    dbMutex.unlock();
}

bool CryptoKernel::Storage::getStatus()
{
    return status;
}

bool CryptoKernel::Storage::store(std::string key, Json::Value value)
{
    std::string dataToStore = toString(value);

    leveldb::WriteOptions options;
    options.sync = true;

    dbMutex.lock();
    leveldb::Status status = db->Put(options, key, dataToStore);
    dbMutex.unlock();

    if(status.ok())
    {
        return true;
    }
    else
    {
        return false;
    }
}

Json::Value CryptoKernel::Storage::get(std::string key)
{
    std::string jsonString;

    leveldb::ReadOptions options;
    options.verify_checksums = true;

    dbMutex.lock();
    db->Get(options, key, &jsonString);
    dbMutex.unlock();

    Json::Value returning = toJson(jsonString);

    return returning;
}

bool CryptoKernel::Storage::erase(std::string key)
{
    dbMutex.lock();
    leveldb::Status status = db->Delete(leveldb::WriteOptions(), key);
    dbMutex.unlock();

    if(status.ok())
    {
        return true;
    }
    else
    {
        return false;
    }
}

CryptoKernel::Storage::Iterator* CryptoKernel::Storage::newIterator()
{
    Iterator* it = new Iterator(db, &dbMutex);
    return it;
}

CryptoKernel::Storage::Iterator::Iterator(leveldb::DB* db, std::mutex* mut)
{
    leveldb::ReadOptions options;
    options.verify_checksums = true;

    dbMutex = mut;
    dbMutex->lock();
    it = db->NewIterator(options);
}

CryptoKernel::Storage::Iterator::~Iterator()
{
    delete it;
    dbMutex->unlock();
}

void CryptoKernel::Storage::Iterator::SeekToFirst()
{
    it->SeekToFirst();
}

void CryptoKernel::Storage::Iterator::Next()
{
    it->Next();
}

bool CryptoKernel::Storage::Iterator::Valid()
{
    return it->Valid();
}

std::string CryptoKernel::Storage::Iterator::key()
{
    return it->key().ToString();
}

Json::Value CryptoKernel::Storage::Iterator::value()
{
    std::string jsonString = it->value().ToString();

    return toJson(jsonString);
}

bool CryptoKernel::Storage::Iterator::getStatus()
{
    return it->status().ok();
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

std::string CryptoKernel::Storage::toString(Json::Value json)
{
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "\t";
    std::string returning = Json::writeString(wbuilder, json);

    return returning;
}

bool CryptoKernel::Storage::destroy(std::string filename)
{
    leveldb::Options options;
    leveldb::DestroyDB(filename, options);

    return true;
}
