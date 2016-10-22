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
