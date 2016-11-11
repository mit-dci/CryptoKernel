#ifndef STORAGETEST_H
#define	STORAGETEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "storage.h"

class StorageTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(StorageTest);

    CPPUNIT_TEST(testGenerateDB);
    CPPUNIT_TEST(testStoreGet);
    CPPUNIT_TEST(testPersistence);
    CPPUNIT_TEST(testErase);
    CPPUNIT_TEST(testToJson);

    CPPUNIT_TEST_SUITE_END();

public:
    StorageTest();
    virtual ~StorageTest();
    void setUp();
    void tearDown();

private:
    void testGenerateDB();
    void testStoreGet();
    void testPersistence();
    void testErase();
    void testToJson();
};

#endif

