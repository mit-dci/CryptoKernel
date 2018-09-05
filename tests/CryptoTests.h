#ifndef CRYPTOTEST_H
#define CRYPTOTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "crypto.h"

class CryptoTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(CryptoTest);

    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testKeygen);
    CPPUNIT_TEST(testSignVerify);
    CPPUNIT_TEST(testPassingKeys);
    CPPUNIT_TEST(testSHA256Hash);
    CPPUNIT_TEST(testSignVerifyInvalid);
    CPPUNIT_TEST_SUITE_END();

public:
    CryptoTest();
    virtual ~CryptoTest();
    void setUp();
    void tearDown();

private:
    void testInit();
    void testKeygen();
    void testSignVerify();
    void testPassingKeys();
    void testSHA256Hash();
    void testSignVerifyInvalid();
    CryptoKernel::Crypto *crypto;
    const std::string plainText = "This is a test.";

};

#endif

