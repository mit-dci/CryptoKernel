#ifndef MATHTEST_H
#define MATHTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "ckmath.h"

class MathTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(MathTest);

    CPPUNIT_TEST(testAdd);
    CPPUNIT_TEST(testSubtract);
    CPPUNIT_TEST(testMultiply);
    CPPUNIT_TEST(testDivide);
    CPPUNIT_TEST(testHexGreater);
    CPPUNIT_TEST(testEmptyOperand);

    CPPUNIT_TEST_SUITE_END();

public:
    MathTest();
    virtual ~MathTest();
    void setUp();
    void tearDown();

private:
    void testAdd();
    void testSubtract();
    void testMultiply();
    void testDivide();
    void testHexGreater();
    void testEmptyOperand();
};

#endif

