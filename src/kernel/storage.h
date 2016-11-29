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
    /**
    * The storage class provide a key-value json storage database
    * interface. This particular implementation uses LevelDB as the
    * underlying storage. It provides functions for saving, retrieving,
    * deleting and iterating over the database.
    */
    class Storage
    {
        public:
            /**
            * Constructs a storage database in the given directory. If no database
            * is found in the given directory then it is created. Otherwise open
            * the existing database.
            *
            * @param filename the directory of the LevelDB database to use
            */
            Storage(const std::string filename);
            
            /**
            * Default destructor, saves and closes the database
            */
            ~Storage();

            /**
            * Stores a json value at the given key string. If the key exists already, overwrite
            * the old value with the new one.
            *
            * @param key the key of the value to store in the database
            * @param value the json value to store in the database
            * @return true if the store operation was successful, false otherwise
            */
            bool store(const std::string key, const Json::Value value);
            
            /**
            * Gets the json value stored at the given key in the database.
            *
            * @param key the key of the json value to be retrieved
            * @return the value stored at the given key or an empty value if the key
            *         has no associated value
            */
            Json::Value get(const std::string key);
            
            /**
            * Erase the given key from the database
            *
            * @param key the key to erase 
            * @return true if the key was erased successfully, false otherwise
            */
            bool erase(const std::string key);

            /**
            * Returns the current status of the storage module
            *
            * @return true if it is safe to continue, false otherwise
            */
            bool getStatus();

            /**
            * Provides an iterator for iterating over keys and values in the database
            */
            class Iterator
            {
                public:
                    /**
                    * Constructs an iterator. While in existence the given db is locked
                    * and other database calls will block until it is deleted
                    *
                    * @param db the LevelDB database object to iterate over
                    * @param dbMutex the mutex associated with the LevelDB mutex
                    */
                    Iterator(leveldb::DB* db, std::mutex* dbMutex);
                    
                    /**
                    * Default deconstructor. Unlocks the database mutex.
                    */
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
                    * @retrn the json value the iterator points to
                    */
                    Json::Value value();

                    /**
                    * Returns the current status of the iterator
                    *
                    * @return true if there are no errors, false if it is not safe to continue
                    */
                    bool getStatus();

                private:
                    leveldb::Iterator* it;
                    std::mutex* dbMutex;
            };

            /**
            * Creates a new iterator to this storage database. Once created,
            * the iterator will block the database for all other methods other
            * than the iterator until it is deleted.
            *
            * @return a pointer to a new iterator object to this database.
            *         Must be deleted once finished with.
            */
            Iterator* newIterator();

            /**
            * Deletes the LevelDB database in the given directory
            *
            * @param filename the directory of the database to delete
            * @return true if the database was deleted successfully, false otherwise
            */
            static bool destroy(const std::string filename);
            
            /**
            * Converts a string to a Json::Value
            *
            * @param json a valid json string to be converted
            * @return a Json::Value representation of the given string
            */
            static Json::Value toJson(const std::string json);

            /**
            * Converts a Json::Value to a json string representation
            *
            * @param json a Json::Value to convert to a string
            * @return a string representation of the given json value
            */
            static std::string toString(const Json::Value json);

        private:
            leveldb::DB* db;
            std::mutex dbMutex;
            bool status;
    };
}

#endif // STORAGE_H_INCLUDED
