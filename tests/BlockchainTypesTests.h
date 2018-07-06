#ifndef BLOCKCHAINTYPESTEST_H
#define BLOCKCHAINTYPESTEST_H

#include <cppunit/extensions/HelperMacros.h>

class BlockchainTypesTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(BlockchainTypesTest);

    CPPUNIT_TEST(testOutputId);
    CPPUNIT_TEST(testTransactionOutputOverflow);

    CPPUNIT_TEST_SUITE_END();

public:
    BlockchainTypesTest();
    virtual ~BlockchainTypesTest();
    void setUp();
    void tearDown();

private:
    void testOutputId();
    void testTransactionOutputOverflow();

};

#endif