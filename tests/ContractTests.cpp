#include "contract.h"

#include "ContractTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ContractTest);

ContractTest::ContractTest() {
    log = new CryptoKernel::Log("testContract.log");
    blockchain = new MyBlockchain(log, 150);
    blockchain->loadChain();
}

ContractTest::~ContractTest() {
    delete blockchain;
}

void ContractTest::setUp() {
}

void ContractTest::tearDown() {
}

void ContractTest::testBasicTrue() {
    const std::string contract = "return true";
    const std::string compressedBytecode = CryptoKernel::ContractRunner::compile(contract);

    CryptoKernel::ContractRunner lvm(blockchain);
    CryptoKernel::Blockchain::transaction tx;
    CryptoKernel::Blockchain::output input1;

    CryptoKernel::Crypto crypto(true);

    Json::Value data;
    data["contract"] = compressedBytecode;

    input1.nonce = 272727;
    input1.value = 100000000;
    input1.publicKey = crypto.getPublicKey();
    input1.data = data;
    input1.id = blockchain->calculateOutputId(input1);

    CryptoKernel::Blockchain::output output1;
    output1.nonce = 283281;
    output1.value = 90000000;
    output1.publicKey = crypto.getPublicKey();
    output1.id = blockchain->calculateOutputId(output1);

    tx.outputs.push_back(output1);
    const std::string outputSetId = blockchain->calculateOutputSetId(tx.outputs);
    input1.signature = crypto.sign(input1.id + outputSetId);

    tx.inputs.push_back(input1);
    tx.timestamp = 123135278;
    tx.id = blockchain->calculateTransactionId(tx);

    CPPUNIT_ASSERT(lvm.evaluateValid(tx));
}
