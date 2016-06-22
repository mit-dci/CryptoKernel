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

    dbMutex.lock();
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, dataToStore);
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

    dbMutex.lock();
    db->Get(leveldb::ReadOptions(), key, &jsonString);
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
    dbMutex = mut;
    dbMutex->lock();
    it = db->NewIterator(leveldb::ReadOptions());
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
