#include "BlockchainTests.h"

#include "blockchain.h"
#include "base64.h"
#include "crypto.h"
#include "schnorr.h"
#include "consensus/regtest.h"
#include "contract.h"
#include "merkletree.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BlockchainTest);

BlockchainTest::BlockchainTest() {
    log.reset(new CryptoKernel::Log("tests.log"));
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
    spendData2["signature"] = schnorr.signSingle(out2.getId().toString() + outputSetId2);
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

    musig_sig_free(sigAgg);
    musig_sig_free(sig1);
    musig_sig_free(sig2);
    musig_pubkey_free(pub);
    musig_key_free(key);
    musig_key_free(key2);
    schnorr_context_free(ctx);
}

void BlockchainTest::testAggregateSignatureWithUnsignedFail() {
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

    std::string msgPayload = out2.getId().toString() + outputSetId2;

    schnorr_context* ctx = schnorr_context_new();

    musig_key* key = musig_key_new(ctx);
    musig_key* key2 = musig_key_new(ctx);

    const std::string decodedKey = base64_decode(schnorr.getPrivateKey());

    CPPUNIT_ASSERT(BN_bin2bn(
            (unsigned char*)decodedKey.c_str(),
            (unsigned int)decodedKey.size(),
            key->a));

    CPPUNIT_ASSERT(EC_POINT_mul(ctx->group, key->pub->A, NULL, ctx->G, key->a, ctx->bn_ctx));

    musig_pubkey* pubkeys[1];
    pubkeys[0] = key->pub;

    musig_sig* sig1;
    musig_pubkey* pub;
    CPPUNIT_ASSERT(musig_sign(ctx, &sig1, &pub, key, pubkeys, 1, (unsigned char*)msgPayload.c_str(), msgPayload.size()));

    const unsigned int buf_len = 65;
    std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

    CPPUNIT_ASSERT_EQUAL(32, BN_bn2binpad(sig1->s, buf.get(), 32));

    CPPUNIT_ASSERT_EQUAL(size_t(33), EC_POINT_point2oct(
            ctx->group,
            sig1->R,
            POINT_CONVERSION_COMPRESSED,
            buf.get() + 32,
            33,
            ctx->bn_ctx));

    const std::string sigStr = base64_encode(buf.get(), buf_len);

    Json::Value aggregateSignature;

    if((*outputSet.begin()).getId() == out2.getId()) {
        aggregateSignature["signs"].append(0);
    } else {
        aggregateSignature["signs"].append(1);
    }
    aggregateSignature["signature"] = sigStr;

    Json::Value spendData2;
    spendData2["aggregateSignature"] = aggregateSignature;
    CryptoKernel::Blockchain::input inp2(out2.getId(), Json::nullValue);
    CryptoKernel::Blockchain::input inp3(out3.getId(), spendData2);
    CryptoKernel::Blockchain::transaction tx2({inp2, inp3}, {out4}, 1530888581);

    const auto res2 = blockchain->submitTransaction(tx2);
    CPPUNIT_ASSERT(!std::get<0>(res2));

    musig_sig_free(sig1);
    musig_pubkey_free(pub);
    musig_key_free(key);
    musig_key_free(key2);
    schnorr_context_free(ctx);
}

void BlockchainTest::testAggregateMixSignature() {
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

    Json::Value outData4;
    outData4["publicKey"] = ECDSAPubKey;

    CryptoKernel::Blockchain::output out2((out.getValue() / 3) - 20000, 0, outData);
    CryptoKernel::Blockchain::output out3((out.getValue() / 3) - 20000, 0, outData2);
    CryptoKernel::Blockchain::output out5((out.getValue() / 3) - 20000, 0, outData4);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({out2, out3, out5}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {out2, out3, out5}, 1530888581);

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

    Json::Value spendData3;
    spendData3["signature"] = crypto.sign(out5.getId().toString() + outputSetId2);

    CryptoKernel::Blockchain::input inp4(out5.getId(), spendData3);

    CryptoKernel::Blockchain::transaction tx2({inp2, inp3, inp4}, {out4}, 1530888581);

    const auto res2 = blockchain->submitTransaction(tx2);
    CPPUNIT_ASSERT(std::get<0>(res2));

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs2 = blockchain->getUnspentOutputs(outData3["publicKey"].asString());
    CPPUNIT_ASSERT_EQUAL(size_t(1), outs2.size());

    musig_sig_free(sigAgg);
    musig_sig_free(sig1);
    musig_sig_free(sig2);
    musig_pubkey_free(pub);
    musig_key_free(key);
    musig_key_free(key2);
    schnorr_context_free(ctx);
}

void BlockchainTest::testSchnorrMultiSignature() {
    CryptoKernel::Crypto crypto(true);

    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    CryptoKernel::Schnorr schnorr;
    CryptoKernel::Schnorr schnorr2;

    const auto aggKey = schnorr.pubkeyAggregate({schnorr.getPublicKey(), schnorr2.getPublicKey()});

    Json::Value outData;
    outData["schnorrKey"] = aggKey;
    CryptoKernel::Blockchain::output out2(out.getValue() - 20000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({out2}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {out2}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT(std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    Json::Value outData3;
    outData3["publicKey"] = "BL2AcSzFw2+rGgQwJ25r7v/misIvr3t4JzkH3U1CCknchfkncSneKLBo6tjnKDhDxZUSPXEKMDtTU/YsvkwxJR8=";
    CryptoKernel::Blockchain::output out4(out2.getValue() - 60000, 0, outData3);

    const auto outputSetId2 = CryptoKernel::Blockchain::transaction::getOutputSetId({out4}).toString();
    const std::string msgPayload = out2.getId().toString() + outputSetId2;

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

    Json::Value spendData2;
    spendData2["signature"] = sigStr;

    CryptoKernel::Blockchain::input inp2(out2.getId(), spendData2);
    CryptoKernel::Blockchain::transaction tx2({inp2}, {out4}, 1530888581);

    const auto res2 = blockchain->submitTransaction(tx2);
    CPPUNIT_ASSERT(std::get<0>(res2));

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs2 = blockchain->getUnspentOutputs(outData3["publicKey"].asString());
    CPPUNIT_ASSERT_EQUAL(size_t(1), outs2.size());

    musig_sig_free(sigAgg);
    musig_sig_free(sig1);
    musig_sig_free(sig2);
    musig_pubkey_free(pub);
    musig_key_free(key);
    musig_key_free(key2);
    schnorr_context_free(ctx);
}

void BlockchainTest::testPayToMerkleRoot() {
    CryptoKernel::Crypto crypto(true);
    std::vector<std::string> spendKeys = {
        "BltFHv7To7EGRpEsCJN4eZQhMUJyKHlSfbxRGKqCYe0=",
        "jiKYKjVDWmEFIIC9VSNDREtVWyX1WgheVT2lS4B4qAE=",
        "t6dSX0XrH330D86PX8HWemjI1f7sA3yfy3eu4HOJubA=",
        "/zfwPyNuaULDkHr44mdOZu5XrM9BqKmXPSoh5kDY2hE="
    };

    std::vector<std::string> spendProofs = {
        "{\"leaves\":[\"4c7f829a1a05269a786fcf8dc990d731a59707a7e1a09bf612b69de40bf6c445\",\"8ce6bc61261ba40e16d67e9f65c5fd983916af4bbb75ab7c7de82abb4b0867c6\",\"24fae9097ff35f6316081c9442f818014ca9ca37961132b251eab4b5458921b5\",\"260e008e75b82690c8c8172d1fbf1c76f5c5582e4a8aba8c06f7a706dd28c0db\"],\"position\":0}",
        "{\"leaves\":[\"fdc9632027116368ddeded54d25809c304d8ac36dc359b4fd9071fa0edbab010\",\"eb9e5706419787823eefb582355b715539adeda7ecf2936eb734c37339ca76cb\",\"12a1df1437deec01365f10169a9994e5e3de2e4790465edbaa2008be4d1755af\",\"260e008e75b82690c8c8172d1fbf1c76f5c5582e4a8aba8c06f7a706dd28c0db\"],\"position\":3}",
        "{\"leaves\":[\"eb9e5706419787823eefb582355b715539adeda7ecf2936eb734c37339ca76cb\",\"fdc9632027116368ddeded54d25809c304d8ac36dc359b4fd9071fa0edbab010\",\"12a1df1437deec01365f10169a9994e5e3de2e4790465edbaa2008be4d1755af\",\"260e008e75b82690c8c8172d1fbf1c76f5c5582e4a8aba8c06f7a706dd28c0db\"],\"position\":2}",
        "{\"leaves\":[\"8ce6bc61261ba40e16d67e9f65c5fd983916af4bbb75ab7c7de82abb4b0867c6\",\"4c7f829a1a05269a786fcf8dc990d731a59707a7e1a09bf612b69de40bf6c445\",\"24fae9097ff35f6316081c9442f818014ca9ca37961132b251eab4b5458921b5\",\"260e008e75b82690c8c8172d1fbf1c76f5c5582e4a8aba8c06f7a706dd28c0db\"],\"position\":1}"
    };

    std::string merkleRoot = "2232b5914a58ec2ef52c2ac979cdbac5417b47f1a0ee01db3893eb140818198";

    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    outData["merkleRoot"] = merkleRoot;
    uint64_t valuePerOutput = out.getValue() / spendKeys.size() - 20000;

    std::set<CryptoKernel::Blockchain::output> p2mrouts = {};
    for(int i = 0; i < spendKeys.size(); i++) {
        CryptoKernel::Blockchain::output p2mrout(valuePerOutput, i, outData);
        p2mrouts.insert(p2mrout);
    }

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId(p2mrouts).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, p2mrouts, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial fanout transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Now, for each key we included in the merkleroot, verify
    // a transaction spending it using that key, back to a pubkey output
    for(int i = 0; i < spendKeys.size(); i++) {
        CryptoKernel::Blockchain::output p2mrout = *std::next(p2mrouts.begin(),i);
        CryptoKernel::Crypto spendCrypto(false);
        spendCrypto.setPrivateKey(spendKeys.at(i));

        Json::Value p2pkOutData;
        p2pkOutData["publicKey"] = spendCrypto.getPublicKey();
        CryptoKernel::Blockchain::output p2pkout(p2mrout.getValue() - 40000, 0, p2pkOutData);

        Json::Value spendData;

        // Parse spendProof
        Json::Value merkleProof;
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        std::string errors;
        bool parsingSuccessful = reader->parse(
            spendProofs.at(i).c_str(),
            spendProofs.at(i).c_str() + spendProofs.at(i).size(),
            &merkleProof,
            &errors
        );
        delete reader;
        CPPUNIT_ASSERT_MESSAGE("Failed parsing proof json", parsingSuccessful);

        spendData["merkleProof"] = merkleProof;
        spendData["pubKeyOrScript"] = spendCrypto.getPublicKey();
        spendData["spendType"] = "pubkey";
        
        const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({p2pkout}).toString();

        spendData["signature"] = spendCrypto.sign(p2mrout.getId().toString() + outputSetId);

        CryptoKernel::Blockchain::input p2mrinp(p2mrout.getId(), spendData);
        CryptoKernel::Blockchain::transaction p2mrspendtx({p2mrinp}, {p2pkout}, 1530888581);

        const auto res = blockchain->submitTransaction(p2mrspendtx);
        CPPUNIT_ASSERT_MESSAGE("Spending p2mr output failed", std::get<0>(res));
    }
}

void BlockchainTest::testPayToMerkleRootScript() {
    CryptoKernel::Crypto crypto(true);
    std::vector<std::string> spendKeys = {
        "BltFHv7To7EGRpEsCJN4eZQhMUJyKHlSfbxRGKqCYe0=",
        "jiKYKjVDWmEFIIC9VSNDREtVWyX1WgheVT2lS4B4qAE=",
        "t6dSX0XrH330D86PX8HWemjI1f7sA3yfy3eu4HOJubA="
    };

    // default example script for sha(preimage) hello world
    std::string spendScript = "BCJNGGBAggEBAAD2BRtMdWFTABmTDQoaCgQIBAgIeFYAAQD1aCh3QAFzcmV0dXJuIHNoYTI1Nih0aGlzSW5wdXRbImRhdGEiXVsicHJlaW1hZ2UiXSkgPT0gIjAzNjc1YWM1M2ZmOWNkMTUzNWNjYzdkZmNkZmEyYzQ1OGM1MjE4MzcxZjQxOGRjMTM2ZjJkMTlhYzFmYmU4YTUigADyKQICCwAAAAYAQABGQEAAR4DAAEfAwAAkgAABXwBBAB4AAIADQAAAAwCAACYAAAEmAIAABQAAAAQHrAAlBAqtACAEBa0AJAQJqwAvFEGlAC1RAQAAAAGlABMLCgAPBAAVAAEAkAEAAAAFX0VOVgAAAAA=";

    std::vector<std::string> spendProofs = {
        "{\"leaves\":[\"97a3d1eaa28520cfd071c92c70e0403d75facf556033d82428eaa2c06942490f\",\"838ea982f21743fb39bfba0e5118351391c5da23902c4d7274668e5be0acc863\",\"d5a2a56f15256cea9205b5a49b0b1b3be695b0b570ce20997eb825933cf9f14f\",\"bb7e85f4193838c48cec68b1bd6b73f0db5e980015e020f0b52e6f2b56c8c3d9\"],\"position\":3}",
        "{\"leaves\":[\"375d47937216d63d0348b55fd32c4d5241a024db8efc7cbd933023026072687f\",\"50b9b6a91268b83e3b04a395b625b573fa43f211b5ad4547451199ab20f214a7\",\"c66ad1cdbe04114a01becfb93b3c6585aa9875bc4eedb78637191eaac9041807\",\"bb7e85f4193838c48cec68b1bd6b73f0db5e980015e020f0b52e6f2b56c8c3d9\"],\"position\":0}",
        "{\"leaves\":[\"50b9b6a91268b83e3b04a395b625b573fa43f211b5ad4547451199ab20f214a7\",\"375d47937216d63d0348b55fd32c4d5241a024db8efc7cbd933023026072687f\",\"c66ad1cdbe04114a01becfb93b3c6585aa9875bc4eedb78637191eaac9041807\",\"bb7e85f4193838c48cec68b1bd6b73f0db5e980015e020f0b52e6f2b56c8c3d9\"],\"position\":1}",
        "{\"leaves\":[\"838ea982f21743fb39bfba0e5118351391c5da23902c4d7274668e5be0acc863\",\"97a3d1eaa28520cfd071c92c70e0403d75facf556033d82428eaa2c06942490f\",\"d5a2a56f15256cea9205b5a49b0b1b3be695b0b570ce20997eb825933cf9f14f\",\"bb7e85f4193838c48cec68b1bd6b73f0db5e980015e020f0b52e6f2b56c8c3d9\"],\"position\":2}"
    };

    std::string merkleRoot = "50145d8b0f8657036bf48482b0571a501fd82156f2dbe4d18bee87e83932aa57";

    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    outData["merkleRoot"] = merkleRoot;
    uint64_t valuePerOutput = out.getValue() / 4 - 20000;

    std::set<CryptoKernel::Blockchain::output> p2mrouts = {};
    for(int i = 0; i < 4; i++) {
        CryptoKernel::Blockchain::output p2mrout(valuePerOutput, i, outData);
        p2mrouts.insert(p2mrout);
    }

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId(p2mrouts).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, p2mrouts, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial fanout transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    // Try spending the output using the script
    CryptoKernel::Blockchain::output p2mrout = *p2mrouts.begin();
    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(p2mrout.getValue() - 90000, 0, p2pkOutData);

    Json::Value scriptSpendData;

    // Parse spendProof
    Json::Value merkleProof;
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    bool parsingSuccessful = reader->parse(
        spendProofs.at(3).c_str(),
        spendProofs.at(3).c_str() + spendProofs.at(3).size(),
        &merkleProof,
        &errors
    );
    delete reader;
    CPPUNIT_ASSERT_MESSAGE("Failed parsing proof json", parsingSuccessful);

    scriptSpendData["merkleProof"] = merkleProof;
    scriptSpendData["pubKeyOrScript"] = spendScript;
    scriptSpendData["spendType"] = "script";
    // This is the data that will satisfy the script!
    scriptSpendData["preimage"] = "Hello, World";

    CryptoKernel::Blockchain::input p2mrinp(p2mrout.getId(), scriptSpendData);
    CryptoKernel::Blockchain::transaction p2mrspendtx({p2mrinp}, {p2pkout}, 1530888581);
    const auto res2 = blockchain->submitTransaction(p2mrspendtx);
    CPPUNIT_ASSERT_MESSAGE("Spending p2mr output failed", std::get<0>(res2));

    // Verify that wrong preimage returns false;

    CryptoKernel::Blockchain::output p2mrout2 = *std::next(p2mrouts.begin(),1);
    Json::Value p2pkOutData2;
    p2pkOutData2["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout2(p2mrout2.getValue() - 90000, 1, p2pkOutData2);

    scriptSpendData["preimage"] = "Hello, World False";

    CryptoKernel::Blockchain::input p2mrinp2(p2mrout2.getId(), scriptSpendData);
    CryptoKernel::Blockchain::transaction p2mrspendtx2({p2mrinp2}, {p2pkout2}, 1530888581);



    const auto res3 = blockchain->submitTransaction(p2mrspendtx2);
    CPPUNIT_ASSERT_MESSAGE("Spending p2mr with wrong preimage did not fail", !std::get<0>(res3));
}

void BlockchainTest::testPayToMerkleRootMalformed() {
    std::string merkleRoot = "2232b5914a58ec2ef52c2ac979cdbac5417b47f1a0ee01db3893eb140818198";
    std::string spendProof = "{\"leaves\":[\"4c7f829a1a05269a786fcf8dc990d731a59707a7e1a09bf612b69de40bf6c445\",\"8ce6bc61261ba40e16d67e9f65c5fd983916af4bbb75ab7c7de82abb4b0867c6\",\"24fae9097ff35f6316081c9442f818014ca9ca37961132b251eab4b5458921b5\",\"260e008e75b82690c8c8172d1fbf1c76f5c5582e4a8aba8c06f7a706dd28c0db\"],\"position\":0}";
    std::string spendKey = "BltFHv7To7EGRpEsCJN4eZQhMUJyKHlSfbxRGKqCYe0=";

    CryptoKernel::Crypto crypto(true);
    
    const auto ECDSAPubKey = crypto.getPublicKey();

    consensus->mineBlock(true, ECDSAPubKey);

    const auto outs = blockchain->getUnspentOutputs(ECDSAPubKey);
    const auto& out = *outs.begin();

    // First, spend our newly acquired coinbase output to
    // a pay-to-merkleroot output
    Json::Value outData;
    outData["merkleRoot"] = merkleRoot;
    CryptoKernel::Blockchain::output p2mrout(out.getValue() - 20000, 0, outData);

    const std::string outputSetId = CryptoKernel::Blockchain::transaction::getOutputSetId({p2mrout}).toString();

    Json::Value spendData;
    spendData["signature"] = crypto.sign(out.getId().toString() + outputSetId);

    CryptoKernel::Blockchain::input inp(out.getId(), spendData);
    CryptoKernel::Blockchain::transaction tx({inp}, {p2mrout}, 1530888581);

    const auto res = blockchain->submitTransaction(tx);
    CPPUNIT_ASSERT_MESSAGE("Initial fanout transaction failed", std::get<0>(res));

    consensus->mineBlock(true, ECDSAPubKey);

    Json::Value p2pkOutData;
    p2pkOutData["publicKey"] = crypto.getPublicKey();
    CryptoKernel::Blockchain::output p2pkout(p2mrout.getValue() - 40000, 0, p2pkOutData);

    
    // Parse spendProof
    Json::Value merkleProof;
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    bool parsingSuccessful = reader->parse(
        spendProof.c_str(),
        spendProof.c_str() + spendProof.size(),
        &merkleProof,
        &errors
    );
    delete reader;
    CPPUNIT_ASSERT_MESSAGE("Failed parsing proof json", parsingSuccessful);

    CryptoKernel::Crypto spendCrypto(false);
    spendCrypto.setPrivateKey(spendKey);
    
    Json::Value validSpendData;
    validSpendData["merkleProof"] = merkleProof;
    validSpendData["pubKeyOrScript"] = spendCrypto.getPublicKey();
    validSpendData["spendType"] = "pubkey";
    const std::string outputSetId2 = CryptoKernel::Blockchain::transaction::getOutputSetId({p2pkout}).toString();
    validSpendData["signature"] = spendCrypto.sign(p2mrout.getId().toString() + outputSetId2);

    Json::Value invalidSpendData = validSpendData;
    invalidSpendData["spendType"] = "bogus";
    const auto res2 = blockchain->submitTransaction(CryptoKernel::Blockchain::transaction({CryptoKernel::Blockchain::input(p2mrout.getId(), invalidSpendData)}, {p2pkout}, 1530888581));
    CPPUNIT_ASSERT_MESSAGE("Invalid spend type did not fail the transaction", !std::get<0>(res2));

    invalidSpendData = validSpendData;
    invalidSpendData["pubKeyOrScript"] = crypto.getPublicKey();
    const auto res3 = blockchain->submitTransaction(CryptoKernel::Blockchain::transaction({CryptoKernel::Blockchain::input(p2mrout.getId(), invalidSpendData)}, {p2pkout}, 1530888581));
    CPPUNIT_ASSERT_MESSAGE("Invalid pub key type did not fail the transaction", !std::get<0>(res3));

    invalidSpendData = validSpendData;
    invalidSpendData["signature"] = spendCrypto.sign("wow");
    const auto res4 = blockchain->submitTransaction(CryptoKernel::Blockchain::transaction({CryptoKernel::Blockchain::input(p2mrout.getId(), invalidSpendData)}, {p2pkout}, 1530888581));
    CPPUNIT_ASSERT_MESSAGE("Invalid signature did not fail the transaction", !std::get<0>(res4));

    invalidSpendData = validSpendData;
    invalidSpendData["merkleProof"]["leaves"][0] = "abcdef";
    const auto res5 = blockchain->submitTransaction(CryptoKernel::Blockchain::transaction({CryptoKernel::Blockchain::input(p2mrout.getId(), invalidSpendData)}, {p2pkout}, 1530888581));
    CPPUNIT_ASSERT_MESSAGE("Invalid merkleProof[0] did not fail the transaction", !std::get<0>(res5));

    invalidSpendData = validSpendData;
    invalidSpendData["merkleProof"]["leaves"][1] = "abcdef";
    const auto res6 = blockchain->submitTransaction(CryptoKernel::Blockchain::transaction({CryptoKernel::Blockchain::input(p2mrout.getId(), invalidSpendData)}, {p2pkout}, 1530888581));
    CPPUNIT_ASSERT_MESSAGE("Invalid merkleProof[1] did not fail the transaction", !std::get<0>(res6));

}