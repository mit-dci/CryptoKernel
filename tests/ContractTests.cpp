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
    blockchain = new MyBlockchain(log);
    consensus = new CryptoKernel::Consensus::PoW::KGW_SHA256(150, blockchain);
    blockchain->loadChain(consensus);
}

void ContractTest::tearDown()
{
    delete blockchain;
    delete log;
    delete consensus;
}

bool ContractTest::runScript(const std::string contract)
{
    const std::string compressedBytecode = CryptoKernel::ContractRunner::compile(contract);

    CryptoKernel::ContractRunner lvm(blockchain);

    CryptoKernel::Crypto crypto(true);

    Json::Value data;
    data["contract"] = compressedBytecode;

    CryptoKernel::Blockchain::input input1(CryptoKernel::BigNum("0"), data);

    data.clear();
    data["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output output1(90000000, 283281, data);

    std::set<CryptoKernel::Blockchain::input> inputs;
    inputs.insert(input1);

    std::set<CryptoKernel::Blockchain::output> outputs;
    outputs.insert(output1);

    CryptoKernel::Blockchain::transaction tx(inputs, outputs, 123135278);

    std::unique_ptr<CryptoKernel::Storage::Transaction> blockchainHandle(blockchain->getTxHandle());

    return lvm.evaluateValid(blockchainHandle.get(), tx);
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
    const std::string contract = "if thisTransaction[\"inputs\"][1][\"nonce\"] == 272727 then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testVerifySignature()
{
    const std::string contract = "local crypto = Crypto.new() crypto:setPublicKey(thisTransaction[\"inputs\"][1][\"publicKey\"]) if crypto:verify(thisTransaction[\"inputs\"][1][\"id\"] .. outputSetId, thisTransaction[\"inputs\"][1][\"signature\"]) then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

void ContractTest::testBlockchainAccess()
{
    const std::string contract = "local json = Json.new() local tip = json:decode(Blockchain.getBlock(\"tip\")) local currentId = tip[\"id\"] for currentHeight = tip[\"height\"], 2, -1 do local nextBlock = json:decode(Blockchain.getBlock(currentId)) currentId = nextBlock[\"previousBlockId\"] end if currentId == \"584ce0855260bb4cbb3b093fe892c747f4b699dcaff247c35ba55dd255d211a1\" then return true else return false end";

    CPPUNIT_ASSERT(runScript(contract));
}

