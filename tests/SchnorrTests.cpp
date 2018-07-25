#include "SchnorrTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(SchnorrTest);

SchnorrTest::SchnorrTest() {
}

SchnorrTest::~SchnorrTest() {
}

void SchnorrTest::setUp() {
    schnorr = new CryptoKernel::Schnorr();
}

void SchnorrTest::tearDown() {
    delete schnorr;
}

/**
* Tests that the Crypto module initialised correctly
*/
void SchnorrTest::testInit() {
    CPPUNIT_ASSERT(schnorr->getStatus());
}

/**
* Tests that a keypair has been generated
*/
void SchnorrTest::testKeygen() {
    const std::string privateKey = schnorr->getPrivateKey();
    const std::string publicKey = schnorr->getPublicKey();

    CPPUNIT_ASSERT(privateKey.size() > 0);
    CPPUNIT_ASSERT(publicKey.size() > 0);
}

/**
* Tests that signing and verifying works
*/
void SchnorrTest::testSignVerify() {
    const std::string privateKey = schnorr->getPrivateKey();
    const std::string publicKey = schnorr->getPublicKey();

    const std::string signature = schnorr->signSingle(plainText);

    CPPUNIT_ASSERT(signature.size() > 0);
    CPPUNIT_ASSERT(schnorr->verify(plainText, signature));
}

/**
* Tests passing key to class
*/
void SchnorrTest::testPassingKeys() {
    CryptoKernel::Schnorr *tempSchnorr = new CryptoKernel::Schnorr();

    const std::string signature = tempSchnorr->signSingle(plainText);
    CPPUNIT_ASSERT(signature.size() > 0);

    CPPUNIT_ASSERT(schnorr->setPublicKey(tempSchnorr->getPublicKey()));
    CPPUNIT_ASSERT(schnorr->verify(plainText, signature));

    delete tempSchnorr;
}

/**
* Tests permuting a valid signature causes a failure  s
*/
void SchnorrTest::testPermutedSigFail() {
    CryptoKernel::Schnorr *tempSchnorr = new CryptoKernel::Schnorr();

    std::string signature = tempSchnorr->signSingle(plainText);
    CPPUNIT_ASSERT(signature.size() > 0);
    const std::string pubKey = tempSchnorr->getPublicKey();

    delete tempSchnorr;

    tempSchnorr = new CryptoKernel::Schnorr();
    CPPUNIT_ASSERT(schnorr->setPublicKey(pubKey));
    CPPUNIT_ASSERT(schnorr->verify(plainText, signature));

    signature.replace(0, 1, "0");
    signature.replace(5, 1, "C");
    signature.replace(10, 1, "D");

    CPPUNIT_ASSERT(!schnorr->verify(plainText, signature));

    delete tempSchnorr;
}

void SchnorrTest::testSamePubkeyAfterSign() {
    const std::string publicKey = schnorr->getPublicKey();

    schnorr->signSingle(plainText);

    CPPUNIT_ASSERT_EQUAL(publicKey, schnorr->getPublicKey());
}
