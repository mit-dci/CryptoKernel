#ifndef CRYPTO_H_INCLUDED
#define CRYPTO_H_INCLUDED

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <string>
#include <sstream>

#define RSA_KEYLEN 2048

namespace CryptoKernel
{

class Crypto
{
public:
    Crypto(bool fGenerate = false);
    ~Crypto();
    bool getStatus();
    std::string decrypt(std::string cipherText);
    std::string encrypt(std::string plainText);
    std::string sign(std::string message);
    bool verify(std::string message, std::string signature);
    std::string getPublicKey();
    std::string getPrivateKey();
    bool setPublicKey(std::string publicKey);
    bool setPrivateKey(std::string privateKey);
    std::string sha256(std::string message);
    std::string getEk();
    bool setEk(std::string Ek);
    unsigned char* sha256uchar(std::string message);

private:
    EVP_PKEY *keypair;
    EVP_CIPHER_CTX *rsaCtx;
    bool status;
    unsigned char *ek;
    int ekl;
    unsigned char *iv;
    int ivl;
};

}

std::string base16_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
bool hex_greater(std::string first, std::string second);
std::string addHex(std::string first, std::string second);

#endif // CRYPTO_H_INCLUDED
