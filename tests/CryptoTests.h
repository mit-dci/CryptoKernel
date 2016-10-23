#ifndef CRYPTOTEST_H
#define	CRYPTOTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "crypto.h"

class CryptoTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(CryptoTest);

    CPPUNIT_TEST(testInit);
    CPPUNIT_TEST(testKeygen);
    CPPUNIT_TEST(testSignVerify);

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
    CryptoKernel::Crypto *crypto;
    
};

#endif

