#ifndef CONTRACTTEST_H
#define CONTRACTTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "blockchain.h"
#include "log.h"

class ContractTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(ContractTest);

    CPPUNIT_TEST(testSimpleFail);
    CPPUNIT_TEST(testHelloWorld);
    CPPUNIT_TEST_SUITE_END();

public:
    ContractTest();
    virtual ~ContractTest();
    void setUp();
    void tearDown();

private:
    class contractTestChain : public CryptoKernel::Blockchain {
        public:
            contractTestChain(CryptoKernel::Log* GlobalLog);
            virtual ~contractTestChain();
        private:
            virtual std::string getCoinbaseOwner(const std::string& publicKey);
            virtual uint64_t getBlockReward(const uint64_t height);
    };

    void testSimpleFail();
    void testHelloWorld();

    std::unique_ptr<CryptoKernel::Blockchain> blockchain;
    std::unique_ptr<CryptoKernel::Log> log;
    std::unique_ptr<CryptoKernel::Consensus::Regtest> consensus;

};

#endif
 