#include "ContractTests.h"
#include "contract.h"
#include "blockchain.h"
#include "base64.h"
#include "crypto.h"
#include "schnorr.h"
#include "consensus/regtest.h"
#include "merkletree.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ContractTest);

ContractTest::contractTestChain::contractTestChain(CryptoKernel::Log* GlobalLog) : CryptoKernel::Blockchain(GlobalLog, "./testblockdb") {}

ContractTest::contractTestChain::~contractTestChain() {}

std::string ContractTest::contractTestChain::getCoinbaseOwner(const std::string& publicKey) {
    return publicKey;
}

uint64_t ContractTest::contractTestChain::getBlockReward(const uint64_t height) {
    return 100000000;
}

ContractTest::ContractTest() {
    log.reset(new CryptoKernel::Log("contracttests.log"));
}

ContractTest::~ContractTest() {}

void ContractTest::setUp() {
    blockchain.reset(new contractTestChain(log.get()));
    consensus.reset(new CryptoKernel::Consensus::Regtest(blockchain.get()));
    blockchain->loadChain(consensus.get(), "genesistest.json");
    consensus->start();
}

void ContractTest::tearDown() {
    blockchain.reset();
    consensus.reset();
    std::remove("genesistest.json");
    CryptoKernel::Storage::destroy("./testblockdb");
}

void ContractTest::testSimpleFail() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    
    // "return false"
    outData["contract"] = "BCJNGGBAglcAAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQD1Aih3QAENcmV0dXJuIGZhbHNlGgBgAgIDAAAABABxJgAAASYAgBYAEwEEAAIaABkBEgCwAAABAAAABV9FTlYAAAAA"; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. Should fail as the script is just 'return false'
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), spendData2);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    const auto res2 = blockchain->submitTransaction(contractspendtx);
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));
}

void ContractTest::testHelloWorld() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    
    // "sha256(preimage) = sha(hello world)"
    outData["contract"] = "BCJNGGBAggEBAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQD1aCh3QAFzcmV0dXJuIHNoYTI1Nih0aGlzSW5wdXRbImRhdGEiXVsicHJlaW1hZ2UiXSkgPT0gIjAzNjc1YWM1M2ZmOWNkMTUzNWNjYzdkZmNkZmEyYzQ1OGM1MjE4MzcxZjQxOGRjMTM2ZjJkMTlhYzFmYmU4YTUigADyKQICCwAAAAYAQABGQEAAR4DAAEfAwAAkgAABXwBBAB4AAIADQAAAAwCAACYAAAEmAIAABQAAAAQHrAAlBAqtACAEBa0AJAQJqwAvFEGlAC1RAQAAAAGlABMLCgAPBAAVAAEAkAEAAAAFX0VOVgAAAAA="; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. First try with invalid preimage. Should fail
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    spendData2["preimage"] = "Hello world FAIL";
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), spendData2);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    const auto res2 = blockchain->submitTransaction(contractspendtx);
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));

    // Try spending from the contract output back to our key. Try with correct preimage. Should succeed
    Json::Value spendData3; // empty
    spendData3["preimage"] = "Hello, World";
    CryptoKernel::Blockchain::input contractin2(contractOutput.getId(), spendData3);
    CryptoKernel::Blockchain::transaction contractspendtx2({contractin2}, {p2pkout}, 1530888581);

    const auto res3 = blockchain->submitTransaction(contractspendtx2);
    CPPUNIT_ASSERT_MESSAGE("Spending contract output failed", std::get<0>(res3));  
}

void ContractTest::testTwoContractInputs() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    
    // "sha256(preimage) = sha(hello world)"
    outData["contract"] = "BCJNGGBAggEBAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQD1aCh3QAFzcmV0dXJuIHNoYTI1Nih0aGlzSW5wdXRbImRhdGEiXVsicHJlaW1hZ2UiXSkgPT0gIjAzNjc1YWM1M2ZmOWNkMTUzNWNjYzdkZmNkZmEyYzQ1OGM1MjE4MzcxZjQxOGRjMTM2ZjJkMTlhYzFmYmU4YTUigADyKQICCwAAAAYAQABGQEAAR4DAAEfAwAAkgAABXwBBAB4AAIADQAAAAwCAACYAAAEmAIAABQAAAAQHrAAlBAqtACAEBa0AJAQJqwAvFEGlAC1RAQAAAAGlABMLCgAPBAAVAAEAkAEAAAAFX0VOVgAAAAA="; 
    
    CryptoKernel::Blockchain::output contractOutput1((out.getValue() / 2) - 90000, 0, outData);
    CryptoKernel::Blockchain::output contractOutput2((out.getValue() / 2) - 90000, 1, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput1, contractOutput2}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput1, contractOutput2}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. First try with invalid preimage. Should fail
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout((contractOutput1.getValue() * 2) - 40000, 0, p2pkOutData);

    Json::Value spendDataFail; 
    spendDataFail["preimage"] = "Hello world FAIL";

    Json::Value spendDataSucceed;
    spendDataSucceed["preimage"] = "Hello, World";

    CryptoKernel::Blockchain::input contractin1(contractOutput1.getId(), spendDataSucceed);
    CryptoKernel::Blockchain::input contractin2(contractOutput2.getId(), spendDataFail);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin1, contractin2}, {p2pkout}, 1530888581);

    const auto res2 = blockchain->submitTransaction(contractspendtx);
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));
}

void ContractTest::testToString() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    Json::Value outData;
    
    // local hello = {} return tostring(hello) == \"table\"
    outData["contract"] = "BCJNGGBAgqsAAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQDxFyh3QAEzbG9jYWwgaGVsbG8gPSB7fSByZXR1cm4gdG9zdHJpbmcoGwDFKSA9PSAidGFibGUiQAD0JQEDCgAAAAsAAABGAEAAgAAAAGSAAAFfQMAAHgAAgENAAABDAIAAZgAAASYAgAACAAAABAlXACEEBk0AEAFHAAFNABMKCgAPBAAVEQaYABcBOgBQBV9FTlYAAAAA"; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. First try with invalid preimage. Should fail
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), Json::nullValue);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    const auto res2 = blockchain->submitTransaction(contractspendtx);
    CPPUNIT_ASSERT_MESSAGE("Spending contract output failed. Shouldn't have.", std::get<0>(res2));
}

void ContractTest::testTableIterationOrder() {
    auto runtest = [&]{
        CryptoKernel::Crypto crypto(true);
        const auto ECDSAPubKey = crypto.getPublicKey();

        consensus->mineBlock(true, ECDSAPubKey);

        const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
        const auto& out = *outs.begin();

        Json::Value outData;
        
        // "hello = {} hello[\"test1\"] = 1 hello[\"test2\"] = 2 hello[\"test3\"] = 3 hello[\"test4\"] = 4 hello[\"test5\"] = 5 res = {} for k, v in pairs(hello) do table.insert(res, v) end return (res[1] == 3 and res[2] == 1 and res[3] == 2 and res[4] == 5 and res[5] == 4)"
        outData["contract"] = "BCJNGGBAghoCAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQDxASh3QAH9aGVsbG8gPSB7fSALANhbInRlc3QxIl0gPSAxEwARMhMAGDITABEzEwAYMxMAETQTABg0EwARNRMAUjUgcmVzaADxA2ZvciBrLCB2IGluIHBhaXJzKC4A8BYpIGRvIHRhYmxlLmluc2VydChyZXMsIHYpIGVuZCByZXR1cm4gFABQWzFdID14ABFhGAAxc1syEAAVMRAAETMQABUyEAARNBAAFTUQABE1EAAlNCkKAfIGAQgxAAAACwAAAAgAAIAGAEAACoDACAAyAMGBEAAiwYIQACLCgxAAIsKEMADxL4CFBgBDAEYAQAAkAAEBHgABgEZBQwBHgcMChsFCAMABAAJkQYABKYAAAKoA/n8GwEIAB4BAAB+AQQAewAOAEACiAEEAH4BAAB7AAhAAUIBBAB8AIAATASAAk0IAH4BCAB7AACAA8RFCAF8AQgAeAACAA0AAAAMAgAAmAAABJgCAAA8AAAAEBlABIQQGygEjEwHlAAIQADkyEwIQADkzEwMQADk0EwQQADQ1EwUQAGEEcmVzBAayASEEBqkBIgQHqgEAYwACZwATMQoADwQArQD1ACAQKKoC8QRnZW5lcmF0b3IpEQAAABkAAAAMGABVc3RhdGUUABEOFAB1Y29udHJvbBYAcAJrEgAAABcKABR2CgCQAQAAAAVfRU5WAAAAAA=="; 
        
        CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

        const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

        Json::Value spendData;
        spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

        CryptoKernel::Blockchain::input inp(out.getId(), spendData);
        CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

        const auto res = blockchain->submitTransaction(tx);
        CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

        consensus->mineBlock(true, ECDSAPubKey);

        // Try spending from the contract output back to our key. First try with invalid preimage. Should fail
        Json::Value p2pkOutData;
        p2pkOutData["publicKey"] = crypto.getPublicKey();
        CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

        Json::Value spendData2; // empty
        CryptoKernel::Blockchain::input contractin(contractOutput.getId(), Json::nullValue);
        CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

        const auto res2 = blockchain->submitTransaction(contractspendtx);
        CPPUNIT_ASSERT_MESSAGE("Spending contract output failed. Shouldn't have.", std::get<0>(res2));
    };

    runtest();

    // run the test multiple times to check order is consistent
    for(int i = 0; i < 10; i++) {
        tearDown();
        setUp();

        runtest();
    }
}

void ContractTest::testSimpleError() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    Json::Value outData;
    
    // "return \"error\""
    outData["contract"] = "BCJNGGBAgl8AAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQD1BCh3QAEPcmV0dXJuICJlcnJvciIcADABAgMGAKEAAAAmAAABJgCADAAhBAYnABMBGwAkAAAlAAYSALAAAAEAAAAFX0VOVgAAAAA="; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. Should fail as the script is just 'return false'
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), spendData2);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    std::tuple<bool, bool> res2;
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("Spending contract caused an exception", res2 = blockchain->submitTransaction(contractspendtx));
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));
}

void ContractTest::testMemoryLimit() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    
    // "tbl = {} for i=0, 1000000, 1 do table.insert(tbl, i) end return true"
    outData["contract"] = "BCJNGGBAgg8BAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQDxCih3QAFFdGJsID0ge30gZm9yIGk9MCwgMTABAPUcLCAxIGRvIHRhYmxlLmluc2VydCh0YmwsIGkpIGVuZCByZXR1cm4gdHJ1ZVIA9D0BBw8AAAALAAAACAAAgAFAAABBgAAAgcAAACgAAYAGAUEAB0FBAkYBQACAAYABJEGAASdA/n8DAIAAJgAAASYAgAAGAAAABAR0YmwTVABCE0BCDwkAEwESACEEBpUAIgQHlgAAFwACGwATDwoADwQAJWEEAAAADCgJAWBuZGV4KQUQAAUUAFVsaW1pdBQAEQsUAEVzdGVwEwAzAmkG/gCQAQAAAAVfRU5WAAAAAA=="; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. Should fail as the script is just 'return false'
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), spendData2);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    std::tuple<bool, bool> res2;
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("Spending contract caused an exception", res2 = blockchain->submitTransaction(contractspendtx));
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));
}

void ContractTest::testInstructionLimit() {
    CryptoKernel::Crypto crypto(true);
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    
    // "for i=0, 100000000, 1 do end return true"
    outData["contract"] = "BCJNGGBAgsQAAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQDzASh3QAEpZm9yIGk9MCwgMTABAPUHLCAxIGRvIGVuZCByZXR1cm4gdHJ1ZTYAMAEECAYA9BUAAABBQAAAgYAAACjA/38ngP9/AwCAACYAAAEmAIAAAwAAABMzAFETAOH1BQkAFAFFAAMEACQAAE0ADwQACWEEAAAADCiqAFBuZGV4KV0AFQUUAFVsaW1pdBQAEQsUAEVzdGVwEwAzAmkERQCQAQAAAAVfRU5WAAAAAA=="; 
    
    CryptoKernel::Blockchain::output contractOutput(out.getValue() - 90000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({contractOutput}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {contractOutput}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial contract transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending from the contract output back to our key. Should fail as the script is just 'return false'
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(contractOutput.getValue() - 40000, 0, p2pkOutData);

    Json::Value spendData2; // empty
    CryptoKernel::Blockchain::input contractin(contractOutput.getId(), spendData2);
    CryptoKernel::Blockchain::transaction contractspendtx({contractin}, {p2pkout}, 1530888581);

    std::tuple<bool, bool> res2;
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("Spending contract caused an exception", res2 = blockchain->submitTransaction(contractspendtx));
    CPPUNIT_ASSERT_MESSAGE("Spending contract output succeeded. Shouldn't have.", !std::get<0>(res2));
}