#ifndef SCHNORRTEST_H
#define SCHNORRTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "schnorr.h"

class SchnorrTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(SchnorrTest);

    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testKeygen);
    CPPUNIT_TEST(testSignVerify);
    CPPUNIT_TEST(testPassingKeys);
    CPPUNIT_TEST(testPermutedSigFail);
    CPPUNIT_TEST(testSamePubkeyAfterSign);

    CPPUNIT_TEST_SUITE_END();

public:
    SchnorrTest();
    virtual ~SchnorrTest();
    void setUp();
    void tearDown();

private:
    void testInit();
    void testKeygen();
    void testSignVerify();
    void testPassingKeys();
    void testPermutedSigFail();
    void testSamePubkeyAfterSign();
    CryptoKernel::Schnorr *schnorr;
    const std::string plainText = "This is a test.";

};

#endif

