#include "StorageTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(StorageTest);

StorageTest::StorageTest() {
}

StorageTest::~StorageTest() {
}

void StorageTest::setUp() {
}

void StorageTest::tearDown() {
    CryptoKernel::Storage::destroy("./testdb");
}

void StorageTest::testGenerateDB() {
    CryptoKernel::Storage database("./testdb");
    
    
    CPPUNIT_ASSERT_EQUAL(true, database.getStatus());
}
