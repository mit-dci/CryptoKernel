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

#include <cschnorr/signature.h>

#include "crypto.h"
#include "base64.h"

CryptoKernel::Schnorr::Schnorr(const bool fGenerate) {
    schnorr_context* ctx = schnorr_context_new();
    schnorr_key* schnorr_key = schnorr_key_new(ctx);

    if(schnorr_key == NULL) {
        throw std::runtime_error("Could not generate key pair");
    }
}

CryptoKernel::Schnorr::~Schnorr() {
    schnorr_context_free(ctx);
    schnorr_key_free(schnorr_key);
}

bool CryptoKernel::Schnorr::verify(const std::string& message,
                                  const std::string& signature) {

    // const std::string messageHash = sha256(message);
    // const std::string decodedSignature = base64_decode(signature);

    if(!schnorr_verify(
        ctx,
        const schnorr_sig* sig,
        schnorr_key.pub,
        reinterpret_cast<unsigned char*>(const_cast<char*>(message)),
        message.length())
    ) {
        return false;
    }

    if(!ECDSA_verify(0, (unsigned char*)messageHash.c_str(), (int)messageHash.size(),
                     (unsigned char*)decodedSignature.c_str(), (int)decodedSignature.size(), eckey)) {
        return false;
    }

    return true;
}

std::string CryptoKernel::Schnorr::sign(const std::string& message) {
    if(schnorr_key != NULL) {

        schnorr_sig** buffer;

        if(!schnorr_sign(ctx,
                         buffer,  // figure out
                         schnorr_key, 
                         reinterpret_cast<unsigned char*>(const_cast<char*>(message)),
                         message.length())) {
            delete[] buffer;
            throw std::runtime_error("Could not sign message");
        } else {
            const std::string returning = base64_encode(buffer, buffer.length());
            delete[] buffer;
            return returning;
        }
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPublicKey() {
    if(schnorr_key != NULL) {
        const std::string returning = base64_encode(schnorr_key.pub, len(schnorr_key.pub));

        return returning;
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPrivateKey() {
    if(schnorr_key != NULL) {
        const std::string returning = base64_encode(schnorr_key.a, len(schnorr_key.a));

        return returning;
    } else {
        return "";
    }
}

// Ask James if we should use schnorr_new_key for both and ask about committed_r_key?
bool CryptoKernel::Schnorr::setPublicKey(const std::string& publicKey) {
    const std::string decodedKey = base64_decode(publicKey);

    if(!EC_KEY_oct2key(eckey, (unsigned char*)decodedKey.c_str(),
                       (unsigned int)decodedKey.size(), NULL)) {
        return false;
    } else {
        return true;
    }
}

bool CryptoKernel::Schnorr::setPrivateKey(const std::string& privateKey) {
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

        return schnorr_key != NULL;
    }
}

bool CryptoKernel::Schnorr::getStatus() {
    return true;
}
