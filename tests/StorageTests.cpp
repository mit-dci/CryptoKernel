#include "StorageTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(StorageTest);

StorageTest::StorageTest() {
    CryptoKernel::Storage::destroy("./testdb");
}

StorageTest::~StorageTest() {
    CryptoKernel::Storage::destroy("./testdb");
}

void StorageTest::setUp() {
}

void StorageTest::tearDown() {
}

void StorageTest::testGenerateDB() {
    CryptoKernel::Storage database("./testdb");
        
    CPPUNIT_ASSERT(database.getStatus());
}

void StorageTest::testStoreGet() {
    CryptoKernel::Storage database("./testdb");

    Json::Value dataToStore;
    dataToStore["myval"] = "this";
    dataToStore["anumber"][0] = 4;
    dataToStore["anumber"][1] = 5;

    CPPUNIT_ASSERT(database.store("mydata", dataToStore));

    CPPUNIT_ASSERT_EQUAL(dataToStore, database.get("mydata"));
}

void StorageTest::testPersistence() {
    CryptoKernel::Storage database("./testdb");

    Json::Value dataToStore;
    dataToStore["myval"] = "this";
    dataToStore["anumber"][0] = 4;
    dataToStore["anumber"][1] = 5;

    CPPUNIT_ASSERT_EQUAL(dataToStore, database.get("mydata"));
}


void StorageTest::testErase() {
    CryptoKernel::Storage database("./testdb");

    CPPUNIT_ASSERT(database.erase("mydata"));

    Json::Value emptyData;

    CPPUNIT_ASSERT_EQUAL(emptyData, database.get("mydata"));
}

void StorageTest::testToJson() {
    Json::Value expected;
    expected["myval"] = "this";
    expected["anumber"][0] = 4;
    expected["anumber"][1] = 5;

    const std::string stringVal = "{\"myval\":\"this\",\"anumber\":[4,5]}"; 

    const Json::Value actual = CryptoKernel::Storage::toJson(stringVal);

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void StorageTest::testToString() {
    Json::Value expected;
    expected["myval"] = "this";
    expected["anumber"][0] = 4;
    expected["anumber"][1] = 5;

    const std::string stringVal = CryptoKernel::Storage::toString(expected);
    
    const Json::Value actual = CryptoKernel::Storage::toJson(stringVal);

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void StorageTest::testIterator() {
    CryptoKernel::Storage database("./testdb");

    Json::Value dataToStore;
    dataToStore["myval"] = "this1";

    database.store("1", dataToStore);
    
    Json::Value dataToStore2;
    dataToStore2["myval"] = "this2";

    database.store("2", dataToStore2);

    CryptoKernel::Storage::Iterator *it = database.newIterator();

    CPPUNIT_ASSERT(it->getStatus());

    it->SeekToFirst();

    CPPUNIT_ASSERT(it->Valid());    
    CPPUNIT_ASSERT_EQUAL(std::string("1"), it->key());
    CPPUNIT_ASSERT_EQUAL(dataToStore, it->value());

    it->Next();

    CPPUNIT_ASSERT(it->Valid());
    CPPUNIT_ASSERT_EQUAL(std::string("2"), it->key());
    CPPUNIT_ASSERT_EQUAL(dataToStore2, it->value());

    it->Next();

    CPPUNIT_ASSERT(!it->Valid());

    delete it;
}
