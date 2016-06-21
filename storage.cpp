#include "storage.h"

CryptoKernel::Storage::Storage(Log *GlobalLog)
{
    log = GlobalLog;
    leveldb::Options options;
    options.create_if_missing = true;

    log->printf(LOG_LEVEL_INFO, "Opening storage database");
    dbMutex.lock();
    leveldb::Status dbstatus = leveldb::DB::Open(options, "./storagedb", &db);
    dbMutex.unlock();

    if(!dbstatus.ok())
    {
        log->printf(LOG_LEVEL_ERR, "Failed to open storage database");
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

bool CryptoKernel::Storage::store(std::string key, std::string value)
{
    dbMutex.lock();
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
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

std::string CryptoKernel::Storage::get(std::string key)
{
    std::string returning;

    dbMutex.lock();
    db->Get(leveldb::ReadOptions(), key, &returning);
    dbMutex.unlock();

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
