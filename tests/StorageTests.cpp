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

void StorageTest::testGetNoExcept() {
    CryptoKernel::Storage database("./testdb", false, 10, true);

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(database.begin());

    Json::Value dbVal;

    CPPUNIT_ASSERT_NO_THROW(dbVal = dbTx->get("mydata"));

    CPPUNIT_ASSERT(dbVal.empty());
}

void StorageTest::testStoreGet() {
    CryptoKernel::Storage database("./testdb", false, 10, true);

    Json::Value dataToStore;
    dataToStore["myval"] = "this";
    dataToStore["anumber"][0] = 4;
    dataToStore["anumber"][1] = 5;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(database.begin());

    CPPUNIT_ASSERT_NO_THROW(dbTx->put("mydata", dataToStore));

    CPPUNIT_ASSERT_EQUAL(dataToStore, dbTx->get("mydata"));

    dbTx->commit();

    dbTx.reset(database.begin());

    CPPUNIT_ASSERT_NO_THROW(dbTx->put("otherdata", dataToStore));

    CPPUNIT_ASSERT_EQUAL(dataToStore, dbTx->get("otherdata"));
}

void StorageTest::testPersistence() {
    CryptoKernel::Storage database("./testdb", false, 10, true);

    Json::Value dataToStore;
    dataToStore["myval"] = "this";
    dataToStore["anumber"][0] = 4;
    dataToStore["anumber"][1] = 5;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(database.begin());

    const Json::Value dbVal = dbTx->get("mydata");

    CPPUNIT_ASSERT_EQUAL(dataToStore, dbVal);

    const Json::Value dbVal2 = dbTx->get("otherdata");

    CPPUNIT_ASSERT(dbVal2.empty());
}


void StorageTest::testErase() {
    CryptoKernel::Storage database("./testdb", false, 10, true);

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(database.begin());

    CPPUNIT_ASSERT_NO_THROW(dbTx->erase("mydata"));

    Json::Value emptyData;

    CPPUNIT_ASSERT_EQUAL(emptyData, dbTx->get("mydata"));
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
    CryptoKernel::Storage database("./testdb", false, 10, true);

    CryptoKernel::Storage::Table myTable("myTable");

    Json::Value dataToStore;
    dataToStore["myval"] = "this1";

    std::unique_ptr<CryptoKernel::Storage::Transaction> transaction(database.begin());

    myTable.put(transaction.get(), "1", dataToStore);

    Json::Value dataToStore2;
    dataToStore2["myval"] = "this2";

    myTable.put(transaction.get(), "2", dataToStore2);

    transaction->commit();

    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new
            CryptoKernel::Storage::Table::Iterator(&myTable, &database));

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
}
