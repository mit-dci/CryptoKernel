/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2016  James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "crypto.h"
#include "base64.h"

CryptoKernel::Crypto::Crypto(const bool fGenerate) {
    eckey = EC_KEY_new();
    if(eckey == NULL) {
        throw std::runtime_error("Could not generate key pair");
    } else {
        ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);
        if(ecgroup == NULL) {
            throw std::runtime_error("Could not generate key pair");
        } else {
            if(!EC_KEY_set_group(eckey, ecgroup)) {
                throw std::runtime_error("Could not generate key pair");
            } else {
                if(fGenerate) {
                    if(!EC_KEY_generate_key(eckey)) {
                        throw std::runtime_error("Could not generate key pair");
                    }
                }
            }
        }
    }
}

CryptoKernel::Crypto::~Crypto() {
    if(ecgroup != NULL) {
        EC_GROUP_free(ecgroup);
    }

    if(eckey != NULL) {
        EC_KEY_free(eckey);
    }
}

bool CryptoKernel::Crypto::verify(std::string message,
                                  std::string signature) {
    const std::string messageHash = sha256(message);
    const std::string decodedSignature = base64_decode(signature);

    if(ECDSA_verify(0, (unsigned char*)messageHash.c_str(), (int)messageHash.size(),
                     (unsigned char*)decodedSignature.c_str(), (int)decodedSignature.size(), eckey) != 1) {
        return false;
    }

    return true;
}

std::string CryptoKernel::Crypto::sign(std::string message) {
    if(EC_KEY_check_key(eckey)) {
        const std::string messageHash = sha256(message);

        unsigned char *buffer, *pp;
        unsigned int buf_len;
        buf_len = ECDSA_size(eckey);
        buffer = new unsigned char[buf_len];
        pp = buffer;

        if(!ECDSA_sign(0, (unsigned char*)messageHash.c_str(), (int)messageHash.size(), pp,
                       &buf_len, eckey)) {
            delete[] buffer;
            throw std::runtime_error("Could not sign message");
        } else {
            const std::string returning = base64_encode(buffer, buf_len);
            delete[] buffer;
            return returning;
        }
    } else {
        return "";
    }
}

std::string CryptoKernel::Crypto::getPublicKey() {
    if(EC_KEY_check_key(eckey)) {    
        unsigned char* publicKey;
        unsigned int keyLen = 0;

        keyLen = EC_KEY_key2buf(eckey, EC_KEY_get_conv_form(eckey), &publicKey, NULL);

        const std::string returning = base64_encode(publicKey, keyLen);
        delete[] publicKey;

        return returning;
    } else {
        return "";
    }
}

std::string CryptoKernel::Crypto::getPrivateKey() {
    if(EC_KEY_check_key(eckey)) {
        unsigned char* privateKey;
        unsigned int keyLen = 0;

        keyLen = EC_KEY_priv2buf(eckey, &privateKey);

        const std::string returning = base64_encode(privateKey, keyLen);
        delete[] privateKey;

        return returning;
    } else {
        return "";
    }
}

bool CryptoKernel::Crypto::setPublicKey(std::string publicKey) {
    const std::string decodedKey = base64_decode(publicKey);

    if(!EC_KEY_oct2key(eckey, (unsigned char*)decodedKey.c_str(),
                       (unsigned int)decodedKey.size(), NULL)) {
        return false;
    } else {
        return true;
    }
}

bool CryptoKernel::Crypto::setPrivateKey(std::string privateKey) {
    const std::string decodedKey = base64_decode(privateKey);

    if(!EC_KEY_oct2priv(eckey, (unsigned char*)decodedKey.c_str(),
                        (unsigned int)decodedKey.size())) {
        throw std::runtime_error("Could not copy private key");
    } else {
        BN_CTX* ctx = BN_CTX_new();
        
        EC_POINT* pub_key = EC_POINT_new(ecgroup);
        if(!EC_POINT_mul(ecgroup, pub_key, 
                     EC_KEY_get0_private_key(eckey), nullptr,
                     nullptr, ctx)) {
            BN_CTX_free(ctx);
            return false;
        }
        
        BN_CTX_free(ctx);
        
        if(!EC_KEY_set_public_key(eckey, pub_key)) {
            EC_POINT_free(pub_key);
            return false;
        }
        
        return EC_KEY_check_key(eckey);
    }
}

std::string CryptoKernel::Crypto::sha256(std::string message) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256CTX;

    if(!SHA256_Init(&sha256CTX)) {
        throw std::runtime_error("Failed to initialise SHA256 context");
    }

    if(!SHA256_Update(&sha256CTX, (unsigned char*)message.c_str(), message.size())) {
        throw std::runtime_error("Failed to calculate SHA256 hash");
    }

    if(!SHA256_Final(hash, &sha256CTX)) {
        throw std::runtime_error("Failed to calculate SHA256 hash");
    }

    return base16_encode(hash, SHA256_DIGEST_LENGTH);
}

bool CryptoKernel::Crypto::getStatus() {
    return true;
}

std::string base16_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::stringstream ss;
    for(unsigned int i = 0; i < in_len; i++) {
        ss << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)bytes_to_encode[i];
    }
    return ss.str();
}

CryptoKernel::AES256::AES256(const Json::Value& objJson) {
    cipherText = objJson["cipherText"].asString();
    
    const std::string decodedIv = base64_decode(objJson["iv"].asString());
    memcpy(&iv, decodedIv.c_str(), decodedIv.size());
    
    const std::string decodedSalt = base64_decode(objJson["salt"].asString());
    memcpy(&salt, decodedSalt.c_str(), decodedSalt.size());
}

CryptoKernel::AES256::AES256(const std::string& password, const std::string& plainText) {
    // Generate salt
    if(!RAND_bytes(salt, sizeof(salt))) {
        throw std::runtime_error("Could not generate random salt");
    }
    
    const auto key = genKey(password);
                      
    // Generate IV
    if(!RAND_bytes(iv, sizeof(iv))) {
        throw std::runtime_error("Could not generate random IV");
    }
    
    // Encrypt plaintext
    EVP_CIPHER_CTX* ctx;
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        throw std::runtime_error("Could not create cipher context");
    }
    
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           key.get(), (unsigned char*)&iv)) {
        throw std::runtime_error("Could not start cipher");
    }
    
    unsigned char cipherText[plainText.size() + 
                             (sizeof(iv) - plainText.size() % sizeof(iv))];
    int len;
    int totalLen;
    
    if(!EVP_EncryptUpdate(ctx, (unsigned char*)&cipherText, &len, 
                          (unsigned char*)plainText.c_str(), plainText.size())) {
        throw std::runtime_error("Could not encrypt data");
    }
    totalLen = len;
    
    if(!EVP_EncryptFinal_ex(ctx, (unsigned char*)&cipherText + len, &len)) {
        throw std::runtime_error("Could not finalise encryption");
    }
    totalLen += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    this->cipherText = base64_encode((unsigned char*)&cipherText, totalLen);
}

Json::Value CryptoKernel::AES256::toJson() const {
    Json::Value returning;
    
    returning["iv"] = base64_encode((unsigned char*)&iv, sizeof(iv));
    returning["salt"] = base64_encode((unsigned char*)&salt, sizeof(salt));
    returning["cipherText"] = cipherText;
    
    return returning;
}

std::string CryptoKernel::AES256::decrypt(const std::string& password) const {  
    const auto key = genKey(password);
                      
    // Encrypt plaintext
    EVP_CIPHER_CTX* ctx;
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        throw std::runtime_error("Could not create cipher context");
    }
    
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           key.get(), (unsigned char*)&iv)) {
        throw std::runtime_error("Could not start cipher");
    }
    
    unsigned char plainText[cipherText.size()];
    int len;
    int totalLen;
    
    const std::string decodedCipherText = base64_decode(cipherText);
    
    if(!EVP_DecryptUpdate(ctx, (unsigned char*)&plainText, &len, 
                          (unsigned char*)decodedCipherText.c_str(), 
                          decodedCipherText.size())) {
        throw std::runtime_error("Could not decrypt data");
    }
    totalLen = len;
    
    if(!EVP_DecryptFinal_ex(ctx, (unsigned char*)&plainText + len, &len)) {
        throw std::runtime_error("Could not finalise decryption");
    }
    totalLen += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    return std::string((char*)&plainText, totalLen);
}

std::shared_ptr<unsigned char> CryptoKernel::AES256::genKey(const std::string& password) const {
    std::shared_ptr<unsigned char> key(new unsigned char[32]);
    
    // Derive key from password
    if(!PKCS5_PBKDF2_HMAC(password.c_str(), password.size(),
                          (unsigned char*)&salt, sizeof(salt),
                          100000,
                          EVP_sha256(),
                          32, key.get())) {
        throw std::runtime_error("Failed to generate key from password");
    }
    
    return key;
}