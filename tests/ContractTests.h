#ifndef CONTRACTTEST_H
#define	CONTRACTTEST_H

#include <memory>
#include <cmath>

#include <cppunit/extensions/HelperMacros.h>

#include "contract.h"
#include "crypto.h"

class MyBlockchain : public CryptoKernel::Blockchain
{
    public:
        MyBlockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime) : CryptoKernel::Blockchain(GlobalLog, blockTime)
        {

        }

        virtual ~MyBlockchain()
        {

        }

    private:
        uint64_t getBlockReward(const uint64_t height)
        {
            if(height > 2)
            {
                return 5000000000 / std::log(height);
            }
            else
            {
                return 5000000000;
            }
        }

        std::string PoWFunction(const std::string inputString)
        {
            CryptoKernel::Crypto crypto;
            return crypto.sha256(inputString);
        }
};


class ContractTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(ContractTest);

    CPPUNIT_TEST(testBasicTrue);
    CPPUNIT_TEST(testInfiniteLoop);
    CPPUNIT_TEST(testCrypto);
    CPPUNIT_TEST(testAccessTx);

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
    bool runScript(const std::string contract);
    MyBlockchain* blockchain;
    CryptoKernel::Log* log;
};

#endif
