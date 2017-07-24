#ifndef CONTRACTTEST_H
#define CONTRACTTEST_H

#include <memory>
#include <cmath>

#include <cppunit/extensions/HelperMacros.h>

#include "contract.h"
#include "crypto.h"
#include "consensus/PoW.h"

class MyBlockchain : public CryptoKernel::Blockchain {
public:
    MyBlockchain(CryptoKernel::Log* GlobalLog) : CryptoKernel::Blockchain(GlobalLog) {

    }

    virtual ~MyBlockchain() {

    }

private:
    uint64_t getBlockReward(const uint64_t height) {
        if(height > 2) {
            return 100000000 / std::log(height);
        } else {
            return 100000000;
        }
    }

    std::string getCoinbaseOwner(const std::string& publicKey) {
        return publicKey;
    }
};


class ContractTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(ContractTest);

    CPPUNIT_TEST(testBasicTrue);
    CPPUNIT_TEST(testInfiniteLoop);
    CPPUNIT_TEST(testCrypto);
    CPPUNIT_TEST(testAccessTx);
    CPPUNIT_TEST(testVerifySignature);
    CPPUNIT_TEST(testBlockchainAccess);

    CPPUNIT_TEST_SUITE_END();

public:
    ContractTest();
    virtual ~ContractTest();
    void setUp();
    void tearDown();

private:
    void testBasicTrue();
    void testInfiniteLoop();
    void testCrypto();
    void testAccessTx();
    void testVerifySignature();
    void testBlockchainAccess();
    bool runScript(const std::string contract);
    MyBlockchain* blockchain;
    CryptoKernel::Consensus::PoW* consensus;
    CryptoKernel::Log* log;
};

#endif
