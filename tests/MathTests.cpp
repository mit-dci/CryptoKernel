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
