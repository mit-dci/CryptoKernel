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
#include <memory>

#include <json/writer.h>
#include <json/reader.h>
#include <leveldb/db.h>

namespace CryptoKernel {
/**
* The storage class provide a key-value json storage database
* interface. This particular implementation uses LevelDB as the
* underlying storage. It provides functions for saving, retrieving,
* deleting and iterating over the database.
*/
class Storage {
public:
    /**
    * Constructs a storage database in the given directory. If no database
    * is found in the given directory then it is created. Otherwise open
    * the existing database.
    *
    * @param filename the directory of the LevelDB database to use
    * @param sync set to true if fsync should take place after every write
    * @param cache 0 turns off the cache, any number higher than zero uses a cache with that size in MB
    * @param bloom set to true to use a bloom filter for lookups
    * @throw std::runtime_error if there is a failure
    */
    Storage(const std::string& filename, const bool sync, const unsigned int cache, const bool bloom);

    /**
    * Default destructor, saves and closes the database
    */
    ~Storage();

    class Transaction {
    public:
        Transaction(Storage* db, const bool readonly = false);
        Transaction(Storage* db, std::recursive_mutex& mut, const bool readonly = false);

        ~Transaction();

        void commit();
        void abort();

        void put(const std::string& key, const Json::Value& data);
        void erase(const std::string& key);
        Json::Value get(const std::string& key);

        bool ended();

        const leveldb::Snapshot* snapshot;

    private:
        struct dbObject {
            Json::Value data;
            bool erased;
        };
        std::map<std::string, dbObject> dbStateCache;
        Storage* db;
        bool finished;
        bool readonly;
        std::recursive_mutex* mut;
    };

    Transaction* begin();

    Transaction* begin(std::recursive_mutex& mut);

    Transaction* beginReadOnly();

    class Table {
    public:
        Table(const std::string& name);

        void put(Transaction* transaction, const std::string& key, const Json::Value& data,
                 const int index = -1);
        void erase(Transaction* transaction, const std::string& key, const int index = -1);
        Json::Value get(Transaction* transaction, const std::string& key, const int index = -1);

        class Iterator {
        public:
            Iterator(Table* table, Storage* db, const leveldb::Snapshot* snapshot = nullptr,
                     const std::string& prefix = "", const int index = -1);

            ~Iterator();

            /**
            * Sets the iterator to the first key in the database
            */
            void SeekToFirst();

            /**
            * Determines whether there are additional keys still in the database
            *
            * @return true if there are keys after the current iterator, false otherwise
            */
            bool Valid();

            /**
            * Shifts the iterator to the next key in the database
            */
            void Next();

            /**
            * Returns the current key the iterator points to
            *
            * @return the key the iterator points to
            */
            std::string key();

            /**
            * Returns the current json value the iterator points to
            *
            * @return the json value the iterator points to
            */
            Json::Value value();
        private:
            leveldb::Iterator* it;
            Table* table;
            Storage* db;
            std::string prefix;
            const leveldb::Snapshot* snapshot;
        };

        std::string getKey(const std::string& key, const int index = -1);
    private:
        std::string tableName;
    };


    /**
    * Deletes the LevelDB database in the given directory
    *
    * @param filename the directory of the database to delete
    * @return true if the database was deleted successfully, false otherwise
    */
    static bool destroy(const std::string& filename);

    /**
    * Converts a string to a Json::Value
    *
    * @param json a valid json string to be converted
    * @return a Json::Value representation of the given string
    */
    static Json::Value toJson(const std::string& json);

    /**
    * Converts a Json::Value to a json string representation
    *
    * @param json a Json::Value to convert to a string
    * @param pretty when true insert tabs and newlines to format the string,
             optional and defaults to false
    * @return a string representation of the given json value
    */
    static std::string toString(const Json::Value& json, const bool pretty = false);

private:
    leveldb::DB* db;
    std::mutex readLock;
    std::mutex writeLock;
    bool sync;
    leveldb::Options options;
};
}

#endif // STORAGE_H_INCLUDED
