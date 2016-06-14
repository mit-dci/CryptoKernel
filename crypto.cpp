#include "crypto.h"
#include "base64.h"
#include <sstream>
#include <algorithm>

CryptoKernel::Crypto::Crypto(bool fGenerate)
{
    keypair = NULL;
    status = true;

    OpenSSL_add_all_digests();

    rsaCtx = new EVP_CIPHER_CTX;

    if(rsaCtx == NULL)
    {
        status = false;
    }

    EVP_CIPHER_CTX_init(rsaCtx);

    if(fGenerate)
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);

        if(EVP_PKEY_keygen_init(ctx) <= 0)
        {
            status = false;
        }

        if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, RSA_KEYLEN) <= 0)
        {
            status = false;
        }

        if(EVP_PKEY_keygen(ctx, &keypair) <= 0)
        {
            status = false;
        }

        EVP_PKEY_CTX_free(ctx);
    }

    ek = new unsigned char[EVP_PKEY_size(keypair)];
    iv = new unsigned char[EVP_MAX_IV_LENGTH];
    if(ek == NULL || iv == NULL)
    {
        status = false;
    }
    ivl = EVP_MAX_IV_LENGTH;
}

CryptoKernel::Crypto::~Crypto()
{
    EVP_PKEY_free(keypair);

    EVP_CIPHER_CTX_cleanup(rsaCtx);

    EVP_cleanup();

    if(ek != NULL)
    {
        delete[] ek;
    }

    if(iv != NULL)
    {
        delete[] iv;
    }

    delete rsaCtx;
}

std::string CryptoKernel::Crypto::encrypt(std::string plainText)
{
    if(!status || plainText == "")
    {
        return "";
    }
    else
    {
        int encMsgLen = 0;
        int blockLen  = 0;

        unsigned char *encMsg;
        encMsg = new unsigned char[plainText.size() + 1 + EVP_MAX_IV_LENGTH];
        if(encMsg == NULL)
        {
            return "";
        }

        if(!EVP_SealInit(rsaCtx, EVP_aes_256_cbc(), &ek, &ekl, iv, &keypair, 1))
        {
            delete[] encMsg;
            return "";
        }

        if(!EVP_SealUpdate(rsaCtx, encMsg + encMsgLen, &blockLen, (const unsigned char*)plainText.c_str(), (int)plainText.size() + 1))
        {
            delete[] encMsg;
            return "";
        }
        encMsgLen += blockLen;

        if(!EVP_SealFinal(rsaCtx, encMsg + encMsgLen, &blockLen))
        {
            delete[] encMsg;
            return "";
        }
        encMsgLen += blockLen;

        EVP_CIPHER_CTX_cleanup(rsaCtx);

        std::string returning = base64_encode(encMsg, encMsgLen);

        delete[] encMsg;

        return returning;
    }
}

std::string CryptoKernel::Crypto::decrypt(std::string cipherText)
{
    if(!status || cipherText == "")
    {
        return "";
    }
    else
    {
        int decLen   = 0;
        int blockLen = 0;

        unsigned char *decMsg;
        decMsg = new unsigned char[cipherText.size() + ivl];
        if(decMsg == NULL)
        {
            return "";
        }

        if(!EVP_OpenInit(rsaCtx, EVP_aes_256_cbc(), ek, ekl, iv, keypair))
        {
            delete[] decMsg;
            return "";
        }

        std::string decodedString = base64_decode(cipherText);

        if(!EVP_OpenUpdate(rsaCtx, decMsg + decLen, &blockLen, (unsigned char*)decodedString.c_str(), (int)decodedString.size()))
        {
            delete[] decMsg;
            return "";
        }
        decLen += blockLen;

        if(!EVP_OpenFinal(rsaCtx, decMsg + decLen, &blockLen))
        {
            delete[] decMsg;
            return "";
        }
        decLen += blockLen;

        EVP_CIPHER_CTX_cleanup(rsaCtx);

        std::string returning((const char*)decMsg, decLen);

        delete[] decMsg;

        return returning;
    }
}

bool CryptoKernel::crypto::verify(std::string message, std::string signature)
{
    if(!status || signature == "" || message == "")
    {
        return false;
    }
    else
    {
        EVP_MD_CTX* ctx = NULL;
        ctx = EVP_MD_CTX_create();

        if(ctx == NULL)
        {
            return false;
        }

        OpenSSL_add_all_algorithms();

        const EVP_MD* md = EVP_get_digestbyname("SHA256");
        if(md == NULL)
        {
            EVP_MD_CTX_cleanup(ctx);
            return false;
        }

        if(!EVP_DigestInit_ex(ctx, md, NULL))
        {
            EVP_MD_CTX_cleanup(ctx);
            return false;
        }

        if(!EVP_DigestVerifyInit(ctx, NULL, md, NULL, keypair))
        {
            EVP_MD_CTX_cleanup(ctx);
            return false;
        }

        if(!EVP_DigestVerifyUpdate(ctx, (unsigned char*)message.c_str(), (int)message.size()))
        {
            EVP_MD_CTX_cleanup(ctx);
            return false;
        }

        std::string decodedString = base64_decode(signature);

        if(!EVP_DigestVerifyFinal(ctx, (unsigned char*)decodedString.c_str(), (int)decodedString.size()))
        {
            EVP_MD_CTX_cleanup(ctx);
            return false;
        }

        EVP_MD_CTX_cleanup(ctx);
        return true;
    }
}

std::string CryptoKernel::Crypto::sign(std::string message)
{
    if(!status || message == "")
    {
        return "";
    }
    else
    {
        EVP_MD_CTX *ctx = NULL;
        ctx = EVP_MD_CTX_create();
        if(ctx == NULL)
        {
            return "";
        }

        OpenSSL_add_all_algorithms();

        const EVP_MD *md = EVP_get_digestbyname("SHA256");
        if(md == NULL)
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        if(!EVP_DigestInit_ex(ctx, md, NULL))
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        if(!EVP_DigestSignInit(ctx, NULL, md, NULL, keypair))
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        if(!EVP_DigestSignUpdate(ctx, (unsigned char*)message.c_str(), (int)message.size()))
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        size_t req = 0;
        if(!EVP_DigestSignFinal(ctx, NULL, &req))
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        if(req < 1)
        {
            EVP_MD_CTX_destroy(ctx);
            return "";
        }

        unsigned char* sig;
        sig = new unsigned char[req];

        if(!EVP_DigestSignFinal(ctx, sig, &req))
        {
            EVP_MD_CTX_destroy(ctx);
            delete[] sig;
            return "";
        }

        EVP_MD_CTX_destroy(ctx);

        std::string returning = base64_encode(sig, req);

        delete[] sig;

        return returning;
    }
}

std::string CryptoKernel::Crypto::getPublicKey()
{
    if(!status || keypair == NULL)
    {
        return "";
    }
    else
    {
        BIO *bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(bio, keypair);

        int pubKeyLen = BIO_pending(bio);
        unsigned char *pubKey;
        pubKey = new unsigned char[pubKeyLen];
        if(pubKey == NULL)
        {
            BIO_free_all(bio);
            return "";
        }

        BIO_read(bio, pubKey, pubKeyLen);

        // Insert the NUL terminator
        pubKey[pubKeyLen-1] = '\0';

        BIO_free_all(bio);

        std::string returning((const char*)pubKey, pubKeyLen);

        delete[] pubKey;

        return returning;
    }
}

std::string CryptoKernel::Crypto::getPrivateKey()
{
    if(!status || keypair == NULL)
    {
        return "";
    }
    else
    {
        BIO *bio = BIO_new(BIO_s_mem());

        PEM_write_bio_PrivateKey(bio, keypair, NULL, NULL, 0, 0, NULL);

        int priKeyLen = BIO_pending(bio);
        unsigned char *priKey;
        priKey = new unsigned char[priKeyLen + 1];
        if(priKey == NULL)
        {
            BIO_free_all(bio);
            return "";
        }

        BIO_read(bio, priKey, priKeyLen);

        // Insert the NUL terminator
        priKey[priKeyLen] = '\0';

        BIO_free_all(bio);

        std::string returning((const char*)priKey, priKeyLen);

        delete[] priKey;

        return returning;
    }
}

bool CryptoKernel::Crypto::setPublicKey(std::string publicKey)
{
    if(!status || publicKey == "")
    {
        return "";
    }
    else
    {
        BIO *bio = BIO_new(BIO_s_mem());
        if(BIO_write(bio, (unsigned char*)publicKey.c_str(), (size_t)publicKey.size()) != (int)publicKey.size())
        {
            BIO_free_all(bio);
            return false;
        }

        PEM_read_bio_PUBKEY(bio, &keypair, NULL, NULL);

        BIO_free_all(bio);

        return true;
    }
}

bool CryptoKernel::Crypto::setPrivateKey(std::string privateKey)
{
    if(!status || privateKey == "")
    {
        return false;
    }
    else
    {
        BIO *bio = BIO_new(BIO_s_mem());
        if(BIO_write(bio, (unsigned char*)privateKey.c_str(), (size_t)privateKey.size()) != (int)privateKey.size())
        {
            BIO_free_all(bio);
            return false;
        }

        PEM_read_bio_PrivateKey(bio, &keypair, NULL, NULL);

        BIO_free_all(bio);

        return true;
    }
}

std::string CryptoKernel::Crypto::sha256(std::string message)
{
    if(message != "")
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];

        SHA256_CTX sha256CTX;

        if(!SHA256_Init(&sha256CTX))
        {
            return "";
        }

        if(!SHA256_Update(&sha256CTX, (unsigned char*)message.c_str(), message.size()))
        {
            return "";
        }

        if(!SHA256_Final(hash, &sha256CTX))
        {
            return "";
        }

        std::string returning = base64_encode(hash, SHA256_DIGEST_LENGTH);

        return returning;
    }
    else
    {
        return "";
    }
}

bool CryptoKernel::Crypto::getStatus()
{
    return status;
}

std::string CryptoKernel::Crypto::getEk()
{
    if(!status || ek == NULL || iv == NULL)
    {
        return "";
    }
    else
    {
        std::string ekString = base64_encode(ek, EVP_PKEY_size(keypair));
        std::string ivString = base64_encode(iv, EVP_MAX_IV_LENGTH);

        std::stringstream stagingString;
        stagingString << ekString << "," << ivString;

        return stagingString.str();
    }
}

bool CryptoKernel::Crypto::setEk(std::string Ek)
{
    if(!status || ek == NULL || iv == NULL)
    {
        return false;
    }
    else
    {
        std::string ekString = base64_decode(Ek.substr(0, Ek.find(",")));
        std::string remaining = Ek.substr(Ek.find(",") + 1, Ek.size());
        ekl = 256;
        std::string ivString = base64_decode(remaining.substr(remaining.find(",") + 1, remaining.size()));

        std::copy(ekString.begin(), ekString.end(), ek);
        std::copy(ivString.begin(), ivString.end(), iv);

        return true;
    }
}
