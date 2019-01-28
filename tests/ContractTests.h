#ifndef CONTRACTTEST_H
#define CONTRACTTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "blockchain.h"
#include "log.h"

class ContractTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(ContractTest);

    CPPUNIT_TEST(testSimpleFail);
    CPPUNIT_TEST(testHelloWorld);
    CPPUNIT_TEST(testTwoContractInputs);
    CPPUNIT_TEST(testToString);
    CPPUNIT_TEST(testTableIterationOrder);
    CPPUNIT_TEST(testSimpleError);
    CPPUNIT_TEST(testMemoryLimit);
    CPPUNIT_TEST(testInstructionLimit);
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
    void testTwoContractInputs();
    void testToString();
    void testTableIterationOrder();
    void testSimpleError();
    void testMemoryLimit();
    void testInstructionLimit();

    std::unique_ptr<CryptoKernel::Blockchain> blockchain;
    std::unique_ptr<CryptoKernel::Log> log;
    std::unique_ptr<CryptoKernel::Consensus::Regtest> consensus;

};

#endif
 