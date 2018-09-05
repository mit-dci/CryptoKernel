#include "CryptoTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(CryptoTest);

CryptoTest::CryptoTest() {
}

CryptoTest::~CryptoTest() {
}

void CryptoTest::setUp() {
    crypto = new CryptoKernel::Crypto(true);
}

void CryptoTest::tearDown() {
    delete crypto;
}

/**
* Tests that the Crypto module initialised correctly
*/
void CryptoTest::testInit() {
    CPPUNIT_ASSERT(crypto->getStatus());
}

/**
* Tests that a keypair has been generated
*/
void CryptoTest::testKeygen() {
    const std::string privateKey = crypto->getPrivateKey();
    const std::string publicKey = crypto->getPublicKey();

    CPPUNIT_ASSERT(privateKey.size() > 0);
    CPPUNIT_ASSERT(publicKey.size() > 0);
}

/**
* Tests that signing and verifying works
*/
void CryptoTest::testSignVerify() {
    const std::string privateKey = crypto->getPrivateKey();
    const std::string publicKey = crypto->getPublicKey();

    const std::string signature = crypto->sign(plainText);

    CPPUNIT_ASSERT(signature.size() > 0);
    CPPUNIT_ASSERT(crypto->verify(plainText, signature));
}

/**
* Tests that verifying invalid texts / signatures / malformed signatures fail
*/
void CryptoTest::testSignVerifyInvalid() {
    const std::string privateKey = crypto->getPrivateKey();
    const std::string publicKey = crypto->getPublicKey();

    const std::string signature = crypto->sign(plainText);

    CPPUNIT_ASSERT(signature.size() > 0);
    // Valid sig, not matching the input
    CPPUNIT_ASSERT(!crypto->verify("invalid plain text", signature));

    // Valid text, invalid sig
    CPPUNIT_ASSERT(!crypto->verify(plainText, "MzA0NTAyMjEwMGVlYmQyOGRmMjljZGYwNGUwODI5ZmZiZmU2ZmFlZTg1YTUxNGI3MjE4YmYxM2RlNjhkM2E0OGMzNGFmMmQxNGUwMjIwMWY2ZDBhN2Y0ZWNmYmUyMjRlMjI1YmFjYTYyOWFhYzYwMDk3MWQwNmQ3Y2Q2ZDNjYzkwZDVkZDExZGFlZTB3OQ=="));

}

/**
* Tests passing key to class
*/
void CryptoTest::testPassingKeys() {
    CryptoKernel::Crypto *tempCrypto = new CryptoKernel::Crypto(true);

    const std::string signature = tempCrypto->sign(plainText);
    CPPUNIT_ASSERT(signature.size() > 0);

    CPPUNIT_ASSERT(crypto->setPublicKey(tempCrypto->getPublicKey()));
    CPPUNIT_ASSERT(crypto->verify(plainText, signature));

    delete tempCrypto;
}

/**
* Tests hashing with SHA256
*/
void CryptoTest::testSHA256Hash() {
    const std::string hash =
        "b6dc933311bc2357cc5fc636a4dbe41a01b7a33b583d043a7f870f3440697e27";
    CPPUNIT_ASSERT_EQUAL(hash, CryptoKernel::Crypto::sha256("wow"));
}
