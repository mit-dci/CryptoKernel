#ifndef STORAGE_H_INCLUDED
#define STORAGE_H_INCLUDED

#include <mutex>
#include <leveldb/db.h>

#include "log.h"

namespace CryptoKernel
{
    class Storage
    {
        public:
            Storage(Log *GlobalLog);
            ~Storage();
            bool store(std::string key, std::string value);
            std::string get(std::string key);
            bool erase(std::string key);
            bool getStatus();

        private:
            leveldb::DB* db;
            std::mutex dbMutex;
            Log *log;
            bool status;
    };
}

#endif // STORAGE_H_INCLUDED
