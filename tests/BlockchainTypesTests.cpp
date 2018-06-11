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
