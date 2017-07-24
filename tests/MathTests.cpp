#include "MathTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(MathTest);

MathTest::MathTest() {
}

MathTest::~MathTest() {
}

void MathTest::setUp() {
}

void MathTest::tearDown() {
}

void MathTest::testAdd() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "16687120699267bed";

    const std::string actual = (CryptoKernel::BigNum(first) + CryptoKernel::BigNum(
                                    second)).toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testSubtract() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "-0f00100220b6000f";

    const std::string actual = (CryptoKernel::BigNum(first) - CryptoKernel::BigNum(
                                    second)).toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testMultiply() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "7d4f42f38def1fb25e7211d89ec16622";

    const std::string actual = (CryptoKernel::BigNum(first) * CryptoKernel::BigNum(
                                    second)).toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testDivide() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "1";

    std::string actual = (CryptoKernel::BigNum(second) / CryptoKernel::BigNum(
                              first)).toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testHexGreater() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";

    CPPUNIT_ASSERT_EQUAL(true, CryptoKernel::BigNum(second) > CryptoKernel::BigNum(first));
    CPPUNIT_ASSERT_EQUAL(false, CryptoKernel::BigNum(first) > CryptoKernel::BigNum(second));
}

void MathTest::testEmptyOperand() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "";
    const std::string expected = "abc381023c383def";

    const std::string actual = (CryptoKernel::BigNum(second) + CryptoKernel::BigNum(
                                    first)).toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}
