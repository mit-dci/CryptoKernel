#ifndef CRYPTO_H_INCLUDED
#define CRYPTO_H_INCLUDED

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <string>

namespace CryptoKernel
{

class Crypto
{
public:
    Crypto(bool fGenerate = false);
    ~Crypto();
    bool getStatus();
    std::string sign(std::string message);
    bool verify(std::string message, std::string signature);
    std::string getPublicKey();
    std::string getPrivateKey();
    bool setPublicKey(std::string publicKey);
    bool setPrivateKey(std::string privateKey);
    std::string sha256(std::string message);

private:
    bool status;
    EC_KEY *eckey;
    EC_GROUP *ecgroup;
};

}

std::string base16_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

#endif // CRYPTO_H_INCLUDED
