#ifndef MERKLETREETEST_H
#define MERKLETREETEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "merkletree.h"

class MerkletreeTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(MerkletreeTest);

    CPPUNIT_TEST(testGetMerkleRoot);
    CPPUNIT_TEST(testGetLeftVal);
    CPPUNIT_TEST(testGetRightVal);
    CPPUNIT_TEST(testMakeTreeFromPtr01);
    CPPUNIT_TEST(testMakeTreeFromPtr02);
    CPPUNIT_TEST(testMakeTreeFromLeaves);
    CPPUNIT_TEST(testMakeProof);
    CPPUNIT_TEST(testVerifyProof);
    CPPUNIT_TEST(testProofSerialize);
    CPPUNIT_TEST(testProofDeserialize);
    CPPUNIT_TEST(testProofDeserializeInvalid);

    CPPUNIT_TEST_SUITE_END();

public:
    MerkletreeTest();
    virtual ~MerkletreeTest();
    void setUp();
    void tearDown();

private:
    void verifyTree();
    
    void testGetTree();
    void testGetMerkleRoot();
    void testGetLeftVal();
    void testGetRightVal();
    void testMakeTreeFromPtr01();
    void testMakeTreeFromPtr02();
    void testMakeTreeFromLeaves();
    void testMakeProof();
    void testVerifyProof();
    void testProofSerialize();
    void testProofDeserialize();
    void testProofDeserializeInvalid();
};

#endif