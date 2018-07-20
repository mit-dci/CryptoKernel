#include "BlockchainTests.h"

#include "blockchain.h"
#include "base64.h"
#include "crypto.h"
#include "schnorr.h"
#include "consensus/regtest.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BlockchainTest);

BlockchainTest::BlockchainTest() {
    log.reset(new CryptoKernel::Log("tests.log", true));
}

BlockchainTest::~BlockchainTest() {}

void BlockchainTest::setUp() {
    blockchain.reset(new testChain(log.get()));
    consensus.reset(new CryptoKernel::Consensus::Regtest(blockchain.get()));
    blockchain->loadChain(consensus.get(), "genesistest.json");
    consensus->start();
}

void BlockchainTest::tearDown() {
    blockchain.reset();
    consensus.reset();
    std::remove("genesistest.json");
    CryptoKernel::Storage::destroy("./testblockdb");
}

BlockchainTest::testChain::testChain(CryptoKernel::Log* GlobalLog) : CryptoKernel::Blockchain(GlobalLog, "./testblockdb") {}

BlockchainTest::testChain::~testChain() {}

std::string BlockchainTest::testChain::getCoinbaseOwner(const std::string& publicKey) {
    return publicKey;
}

uint64_t BlockchainTest::testChain::getBlockReward(const uint64_t height) {
    return 100000000;
}

void BlockchainTest::testVerifyMalformedSignature() {
    consensus->mineBlock(true, "BL2AcSzFw2+rGgQwJ25r7v/misIvr3t4JzkH3U1CCknchfkncSneKLBo6tjnKDhDxZUSPXEKMDtTU/YsvkwxJR8=");

    const auto block = blockchain->getBlockByHeight(2);

    const auto output = *block.getCoinbaseTx().getOutputs().begin();

    CryptoKernel::Blockchain::output outp(output.getValue() - 20000, 0, Json::nullValue);

    Json::Value spendData;
    Json::Value randomData;
    randomData["this is"] = "malformed";

    // Something malformed (not a string)
    spendData["signature"] = randomData;

    CryptoKernel::Blockchain::input inp(output.getId(), spendData);

    CryptoKernel::Blockchain::transaction tx({inp}, {outp}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);

    CPPUNIT_ASSERT(!std::get<0>(res));
}

void BlockchainTest::testListUnspentOutputsFromCoinbase() {
    const std::string pubKey = "BL2AcSzFw2+rGgQwJ25r7v/misIvr3t4JzkH3U1CCknchfkncSneKLBo6tjnKDhDxZUSPXEKMDtTU/YsvkwxJR8=";

    // Mine three blocks
    for(int i = 0; i < 3; i++) {
        consensus->mineBlock(true, pubKey);
    }

    // Ensure three blocks were actually mined
    blockchain->getBlockByHeight(4);

    const auto outs = blockchain->getUnspentOutputs(pubKey);

    CPPUNIT_ASSERT_EQUAL(size_t(3), outs.size());

    for(const auto& out : outs) {
        CPPUNIT_ASSERT_EQUAL(pubKey, out.getData()["publicKey"].asString());
        CPPUNIT_ASSERT_EQUAL(uint64_t(100000000), out.getValue());
    }
}

void BlockchainTest::testSingleSchnorrSignature() {
    CryptoKernel::Crypto crypto(true);

    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    CryptoKernel::Schnorr schnorr;

    Json::Value outData;
    outData["schnorrKey"] = schnorr.getPublicKey();

    CryptoKernel::Blockchain::output out2(out.getValue() - 20000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({out2}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {out2}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT(std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    Json::Value outData2;
    outData2["publicKey"] = "BL2AcSzFw2+rGgQwJ25r7v/misIvr3t4JzkH3U1CCknchfkncSneKLBo6tjnKDhDxZUSPXEKMDtTU/YsvkwxJR8=";
    CryptoKernel::Blockchain::output out3(out2.getValue() - 20000, 0, outData2);

    const auto outputSetId2 = CryptoKernel::Blockchain::transaction::getOutputSetId({out3}).toString();

    Json::Value spendData2;
    spendData2["signature"] = schnorr.sign(out2.getId().toString() + outputSetId2);
    CryptoKernel::Blockchain::input inp2(out2.getId(), spendData2);
    CryptoKernel::Blockchain::transaction tx2({inp2}, {out3}, 1530888581);

    const auto res2 = blockchain->submitTransaction(tx2);
    CPPUNIT_ASSERT(std::get<0>(res2));

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs2 = blockchain->getUnspentOutputs(outData2["publicKey"].asString());
    CPPUNIT_ASSERT_EQUAL(size_t(1), outs2.size());
}

void BlockchainTest::testAggregateSchnorrSignature() {
    CryptoKernel::Crypto crypto(true);

    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    CryptoKernel::Schnorr schnorr;
    CryptoKernel::Schnorr schnorr2;

    Json::Value outData;
    outData["schnorrKey"] = schnorr.getPublicKey();

    Json::Value outData2;
    outData2["schnorrKey"] = schnorr2.getPublicKey();

    CryptoKernel::Blockchain::output out2((out.getValue() / 2) - 20000, 0, outData);
    CryptoKernel::Blockchain::output out3((out.getValue() / 2) - 20000, 0, outData2);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({out2, out3}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {out2, out3}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT(std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    Json::Value outData3;
    outData3["publicKey"] = "BL2AcSzFw2+rGgQwJ25r7v/misIvr3t4JzkH3U1CCknchfkncSneKLBo6tjnKDhDxZUSPXEKMDtTU/YsvkwxJR8=";
    CryptoKernel::Blockchain::output out4(out2.getValue() - 60000, 0, outData3);

    const auto outputSetId2 = CryptoKernel::Blockchain::transaction::getOutputSetId({out4}).toString();

    std::set<CryptoKernel::Blockchain::output> outputSet({out2, out3});

    std::string msgPayload;
    for(const auto& o : outputSet) {
        msgPayload += o.getId().toString();
    }
    msgPayload += outputSetId2;

    std::cout << msgPayload;

    schnorr_context* ctx = schnorr_context_new();

    musig_key* key = musig_key_new(ctx);
    musig_key* key2 = musig_key_new(ctx);

    const std::string decodedKey = base64_decode(schnorr.getPrivateKey());

    CPPUNIT_ASSERT(BN_bin2bn(
            (unsigned char*)decodedKey.c_str(),
            (unsigned int)decodedKey.size(),
            key->a));

    const std::string decodedKey2 = base64_decode(schnorr2.getPrivateKey());

    CPPUNIT_ASSERT(BN_bin2bn(
            (unsigned char*)decodedKey2.c_str(),
            (unsigned int)decodedKey2.size(),
            key2->a));

    CPPUNIT_ASSERT(EC_POINT_mul(ctx->group, key->pub->A, NULL, ctx->G, key->a, ctx->bn_ctx));
    CPPUNIT_ASSERT(EC_POINT_mul(ctx->group, key2->pub->A, NULL, ctx->G, key2->a, ctx->bn_ctx));

    musig_pubkey* pubkeys[2];

    std::set<std::string> pubkeySet({schnorr.getPublicKey(), schnorr2.getPublicKey()});
    if(*pubkeySet.begin() == schnorr.getPublicKey()) {
        pubkeys[0] = key->pub;
        pubkeys[1] = key2->pub;
    } else {
        pubkeys[1] = key->pub;
        pubkeys[0] = key2->pub;
    }

    musig_sig* sig1;
    musig_sig* sig2;
    musig_pubkey* pub;
    CPPUNIT_ASSERT(musig_sign(ctx, &sig1, &pub, key, pubkeys, 2, (unsigned char*)msgPayload.c_str(), msgPayload.size()));
    CPPUNIT_ASSERT(musig_sign(ctx, &sig2, &pub, key2, pubkeys, 2, (unsigned char*)msgPayload.c_str(), msgPayload.size()));

    musig_sig* sigs[2];
    sigs[0] = sig1;
    sigs[1] = sig2;

    musig_sig* sigAgg;
    CPPUNIT_ASSERT(musig_aggregate(ctx, &sigAgg, sigs, 2));

    const unsigned int buf_len = 65;
    std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

    CPPUNIT_ASSERT_EQUAL(32, BN_bn2binpad(sigAgg->s, buf.get(), 32));

    CPPUNIT_ASSERT_EQUAL(size_t(33), EC_POINT_point2oct(
            ctx->group,
            sigAgg->R,
            POINT_CONVERSION_COMPRESSED,
            buf.get() + 32,
            33,
            ctx->bn_ctx));

    const std::string sigStr = base64_encode(buf.get(), buf_len);

    Json::Value aggregateSignature;
    aggregateSignature["signs"].append(0);
    aggregateSignature["signs"].append(1);
    aggregateSignature["signature"] = sigStr;

    Json::Value spendData2;
    spendData2["aggregateSignature"] = aggregateSignature;
    CryptoKernel::Blockchain::input inp2(out2.getId(), Json::nullValue);
    CryptoKernel::Blockchain::input inp3(out3.getId(), spendData2);
    CryptoKernel::Blockchain::transaction tx2({inp2, inp3}, {out4}, 1530888581);

    const auto res2 = blockchain->submitTransaction(tx2);
    CPPUNIT_ASSERT(std::get<0>(res2));

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs2 = blockchain->getUnspentOutputs(outData3["publicKey"].asString());
    CPPUNIT_ASSERT_EQUAL(size_t(1), outs2.size());
}
