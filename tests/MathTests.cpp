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
    
    std::string actual = CryptoKernel::Math::addHex(first, second);
    
    actual.erase(0, std::min(actual.find_first_not_of('0'), actual.size()-1));
    
    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testSubtract() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "f00100220b6000f";
    
    std::string actual = CryptoKernel::Math::subtractHex(second, first);
    
    actual.erase(0, std::min(actual.find_first_not_of('0'), actual.size()-1));
    
    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testMultiply() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "7d4f42f38def1fb25e7211d89ec16622";

    const std::string actual = CryptoKernel::Math::multiplyHex(second, first);

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MathTest::testDivide() {
    const std::string first = "aBc381023c383Def";
    const std::string second = "bAc391045cEE3Dfe";
    const std::string expected = "1";

    std::string actual = CryptoKernel::Math::divideHex(second, first);
    
    actual.erase(0, std::min(actual.find_first_not_of('0'), actual.size()-1));

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}
