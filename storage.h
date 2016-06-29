#ifndef STORAGE_H_INCLUDED
#define STORAGE_H_INCLUDED

#include <mutex>

#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/reader.h>
#include <leveldb/db.h>

namespace CryptoKernel
{
    class Storage
    {
        public:
            Storage(std::string filename);
            ~Storage();
            bool store(std::string key, Json::Value value);
            Json::Value get(std::string key);
            bool erase(std::string key);
            bool getStatus();
            class Iterator
            {
                public:
                    Iterator(leveldb::DB* db, std::mutex* dbMutex);
                    ~Iterator();
                    void SeekToFirst();
                    bool Valid();
                    void Next();
                    std::string key();
                    Json::Value value();
                    bool getStatus();

                private:
                    leveldb::Iterator* it;
                    std::mutex* dbMutex;
            };
            Iterator* newIterator();

            static bool destroy(std::string filename);
            static Json::Value toJson(std::string json);
            static std::string toString(Json::Value json);

        private:
            leveldb::DB* db;
            std::mutex dbMutex;
            bool status;
    };
}

#endif // STORAGE_H_INCLUDED
