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
        
    CPPUNIT_ASSERT_EQUAL(true, database.getStatus());
}

void StorageTest::testStoreGet() {
    CryptoKernel::Storage database("./testdb");

    Json::Value dataToStore;
    dataToStore["myval"] = "this";
    dataToStore["anumber"][0] = 4;
    dataToStore["anumber"][1] = 5;

    CPPUNIT_ASSERT_EQUAL(true, database.store("mydata", dataToStore));

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

    CPPUNIT_ASSERT_EQUAL(true, database.erase("mydata"));

    Json::Value emptyData;

    CPPUNIT_ASSERT_EQUAL(emptyData, database.get("mydata"));
}

