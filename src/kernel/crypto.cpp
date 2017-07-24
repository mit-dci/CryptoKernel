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

#include <openssl/sha.h>

#include "crypto.h"
#include "base64.h"

CryptoKernel::Crypto::Crypto(const bool fGenerate) {
    eckey = EC_KEY_new();
    if(eckey == NULL) {
        status = false;
    } else {
        ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);
        if(ecgroup == NULL) {
            status = false;
        } else {
            if(!EC_KEY_set_group(eckey, ecgroup)) {
                status = false;
            } else {
                status = true;
                if(fGenerate) {
                    if(!EC_KEY_generate_key(eckey)) {
                        status = false;
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

bool CryptoKernel::Crypto::verify(const std::string& message,
                                  const std::string& signature) {
    if(!status || signature == "" || message == "") {
        return false;
    } else {
        std::string messageHash = sha256(message);
        std::string decodedSignature = base64_decode(signature);

        if(!ECDSA_verify(0, (unsigned char*)messageHash.c_str(), (int)messageHash.size(),
                         (unsigned char*)decodedSignature.c_str(), (int)decodedSignature.size(), eckey)) {
            return false;
        }

        return true;
    }
}

std::string CryptoKernel::Crypto::sign(const std::string& message) {
    if(!status || message == "") {
        return "";
    } else {
        std::string messageHash = sha256(message);

        unsigned char *buffer, *pp;
        unsigned int buf_len;
        buf_len = ECDSA_size(eckey);
        buffer = new unsigned char[buf_len];
        pp = buffer;

        if(!ECDSA_sign(0, (unsigned char*)messageHash.c_str(), (int)messageHash.size(), pp,
                       &buf_len, eckey)) {
            delete[] buffer;
            return "";
        } else {
            std::string returning = base64_encode(buffer, buf_len);
            delete[] buffer;
            return returning;
        }
    }
}

std::string CryptoKernel::Crypto::getPublicKey() {
    if(!status) {
        return "";
    } else {
        unsigned char* publicKey;
        unsigned int keyLen = 0;

        keyLen = EC_KEY_key2buf(eckey, EC_KEY_get_conv_form(eckey), &publicKey, NULL);

        std::string returning = base64_encode(publicKey, keyLen);
        delete[] publicKey;

        return returning;
    }
}

std::string CryptoKernel::Crypto::getPrivateKey() {
    if(!status) {
        return "";
    } else {
        unsigned char* privateKey;
        unsigned int keyLen = 0;

        keyLen = EC_KEY_priv2buf(eckey, &privateKey);

        std::string returning = base64_encode(privateKey, keyLen);
        delete[] privateKey;

        return returning;
    }
}

bool CryptoKernel::Crypto::setPublicKey(const std::string& publicKey) {
    if(!status || publicKey == "") {
        return false;
    } else {
        std::string decodedKey = base64_decode(publicKey);

        if(!EC_KEY_oct2key(eckey, (unsigned char*)decodedKey.c_str(),
                           (unsigned int)decodedKey.size(), NULL)) {
            status = false;
            return false;
        } else {
            status = true;
            return true;
        }
    }
}

bool CryptoKernel::Crypto::setPrivateKey(const std::string& privateKey) {
    if(!status || privateKey == "") {
        return false;
    } else {
        std::string decodedKey = base64_decode(privateKey);

        if(!EC_KEY_oct2priv(eckey, (unsigned char*)decodedKey.c_str(),
                            (unsigned int)decodedKey.size())) {
            status = false;
            return false;
        } else {
            status = true;
            return true;
        }
    }
}

std::string CryptoKernel::Crypto::sha256(const std::string& message) {
    if(message != "") {
        unsigned char hash[SHA256_DIGEST_LENGTH];

        SHA256_CTX sha256CTX;

        if(!SHA256_Init(&sha256CTX)) {
            return "";
        }

        if(!SHA256_Update(&sha256CTX, (unsigned char*)message.c_str(), message.size())) {
            return "";
        }

        if(!SHA256_Final(hash, &sha256CTX)) {
            return "";
        }

        std::string returning = base16_encode(hash, SHA256_DIGEST_LENGTH);

        return returning;
    } else {
        return "";
    }
}

bool CryptoKernel::Crypto::getStatus() {
    return status;
}

std::string base16_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::stringstream ss;
    for(unsigned int i = 0; i < in_len; i++) {
        ss << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)bytes_to_encode[i];
    }
    return ss.str();
}
