#ifndef BLOCKCHAINTEST_H
#define BLOCKCHAINTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "blockchain.h"

class BlockchainTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(BlockchainTest);

    CPPUNIT_TEST(testVerifyMalformedSignature);
    CPPUNIT_TEST(testListUnspentOutputsFromCoinbase);
    CPPUNIT_TEST(testSingleSchnorrSignature);
    CPPUNIT_TEST(testAggregateSchnorrSignature);
    CPPUNIT_TEST(testAggregateSignatureWithUnsignedFail);
    CPPUNIT_TEST(testAggregateMixSignature);
    CPPUNIT_TEST(testSchnorrMultiSignature);
    CPPUNIT_TEST(testPayToMerkleRoot);
    CPPUNIT_TEST(testPayToMerkleRootScript);
    CPPUNIT_TEST(testPayToMerkleRootMalformed);
    CPPUNIT_TEST_SUITE_END();

public:
    BlockchainTest();
    virtual ~BlockchainTest();
    void setUp();
    void tearDown();

private:
    class testChain : public CryptoKernel::Blockchain {
        public:
            testChain(CryptoKernel::Log* GlobalLog);
            virtual ~testChain();
        private:
            virtual std::string getCoinbaseOwner(const std::string& publicKey);
            virtual uint64_t getBlockReward(const uint64_t height);
    };
 

    void testVerifyMalformedSignature();
    void testListUnspentOutputsFromCoinbase();
    void testSingleSchnorrSignature();
    void testAggregateSchnorrSignature();
    void testAggregateSignatureWithUnsignedFail();
    void testAggregateMixSignature();
    void testSchnorrMultiSignature();
    void testPayToMerkleRoot();
    void testPayToMerkleRootScript();
    void testPayToMerkleRootMalformed();

    
    std::unique_ptr<CryptoKernel::Blockchain> blockchain;
    std::unique_ptr<CryptoKernel::Log> log;
    std::unique_ptr<CryptoKernel::Consensus::Regtest> consensus;

};

#endif
