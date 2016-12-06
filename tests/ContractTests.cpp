#include "contract.h"

#include "ContractTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ContractTest);

ContractTest::ContractTest()
{

}

ContractTest::~ContractTest()
{

}

void ContractTest::setUp()
{
    log = new CryptoKernel::Log("testContract.log");
    blockchain = new MyBlockchain(log, 150);
    blockchain->loadChain();
}

void ContractTest::tearDown()
{
    delete blockchain;
}

bool ContractTest::runScript(const std::string contract)
{
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

    return lvm.evaluateValid(tx);
}

void ContractTest::testBasicTrue()
{
    const std::string contract = "return true";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testInfiniteLoop()
{
    const std::string contract = "while true do end return true";

    CPPUNIT_ASSERT(!runScript(contract));
}

void ContractTest::testCrypto()
{
    const std::string contract = "local genCrypto = Crypto.new(true) local publicKey = genCrypto:getPublicKey() local signature = genCrypto:sign(\"this is a test signature\") local crypto = Crypto.new() crypto:setPublicKey(publicKey) if crypto:verify(\"this is a test signature\", signature) then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testAccessTx()
{
    const std::string contract = "local json = Json.new() local tx = json:decode(txJson) if tx[\"inputs\"][1][\"nonce\"] == 272727 then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testVerifySignature()
{
    const std::string contract = "local json = Json.new() local tx = json:decode(txJson) local crypto = Crypto.new() crypto:setPublicKey(tx[\"inputs\"][1][\"publicKey\"]) if crypto:verify(tx[\"inputs\"][1][\"id\"] .. outputSetId, tx[\"inputs\"][1][\"signature\"]) then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testBlockchainAccess()
{
    const std::string contract = "local json = Json.new() local tip = json:decode(Blockchain.getBlock(\"tip\")) local currentId = tip[\"id\"] for currentHeight = tip[\"height\"], 2, -1 do local nextBlock = json:decode(Blockchain.getBlock(currentId)) currentId = nextBlock[\"previousBlockId\"] end if currentId == \"9344a9cab3084e15b2f1b5572b9a2fcf56920b51afad8f77de814dd194f96e90\" then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}
