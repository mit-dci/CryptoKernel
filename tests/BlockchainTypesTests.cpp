#include "BlockchainTypesTests.h"

#include "blockchain.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BlockchainTypesTest);

BlockchainTypesTest::BlockchainTypesTest() {}

BlockchainTypesTest::~BlockchainTypesTest() {}

void BlockchainTypesTest::setUp() {}

void BlockchainTypesTest::tearDown() {}

/**
* Tests that outputs have the correct ID
*/
void BlockchainTypesTest::testOutputId() {
    Json::Value data;
    data["publicKey"] = "BMoEeFbdyC8blWvlklSJ2oKRjEJfcq08+HZkmQW1ICJpC7nebygMt5AXhXDiwHuEF4KlHuJBwNGatpKifhoqp4s=";

    CryptoKernel::Blockchain::output out(8081988463, 4062896946, data);

    CPPUNIT_ASSERT_EQUAL(std::string("ff8840289b59187d16521ddde6b19de1c1b8994220a4dbb112f5978b34605b17"),
    out.getId().toString());
}

/**
 * Tests that a transactions where the output total overflows a uint64 is
 * invalid
 */
void BlockchainTypesTest::testTransactionOutputOverflow() {
    CryptoKernel::BigNum outputToSpend("fffa934e3065e856e16c2f4ee0ec1591f4b80e5150e7cd3c75714d5f8dba2bb3");

    CryptoKernel::Blockchain::input inp(outputToSpend, Json::nullValue);
    CryptoKernel::Blockchain::output out1(std::numeric_limits<uint64_t>::max(), 0, Json::nullValue);
    CryptoKernel::Blockchain::output out2(10, 0, Json::nullValue);

    CPPUNIT_ASSERT_THROW(CryptoKernel::Blockchain::transaction({inp}, {out1, out2}, 1), CryptoKernel::Blockchain::InvalidElementException);
}