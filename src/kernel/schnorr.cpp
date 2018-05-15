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

#include "crypto.h"
#include "schnorr.h"
#include "base64.h"

// added
#include <openssl/sha.h>


CryptoKernel::Schnorr::Schnorr() {
    this->ctx = schnorr_context_new();
    this->key = musig_key_new(ctx);

    if(key == NULL) {
        throw std::runtime_error("Could not generate key pair");
    }
}

CryptoKernel::Schnorr::~Schnorr() {
    // TODO fix segfault on schnorr_context_free, write musig_free
    schnorr_context_free(ctx);
    //musig_free(key);
}

bool CryptoKernel::Schnorr::verify(const std::string& message,
                                  const std::string& signature) {
    CryptoKernel::Crypto crypto;
    const std::string messageHash = crypto.sha256(message);
    const std::string decodedSignature = base64_decode(signature);

    musig_sig* sig;

    BN_hex2bn(&sig->s, (const char*)decodedSignature.c_str());
    EC_POINT_hex2point(ctx->group, (const char*)messageHash.c_str(), sig->R, ctx->bn_ctx);

    // free(messageHash);
    // free(decodedSignature);

    if(!musig_verify(
        ctx,
        sig,
        key->pub,
        (unsigned char*)message.c_str(),
        message.length())
    ) {
        return false;
    }

    return true;
}

std::string CryptoKernel::Schnorr::sign(const std::string& message) {
    if(key != NULL) {
        musig_sig* sig;
        musig_pubkey* pubkeys[1];
        pubkeys[0] = key->pub;

        if(!musig_sign(ctx,
            &sig,
            &key->pub,
            key,
            pubkeys,
            sizeof(pubkeys),
            (unsigned char*)message.c_str(),
            message.size())) {
            delete[] sig;
            throw std::runtime_error("Could not sign message");
        } else {
            const std::string returning = base64_encode((const unsigned char*)BN_bn2hex(sig->s), static_cast<int>(sizeof(sig)));
            delete[] sig;
            return returning;
        }
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPublicKey() {
    if(key != NULL) {
        unsigned char *buf;

        if (EC_POINT_point2buf(ctx->group, key->pub->A,
                           POINT_CONVERSION_COMPRESSED,
                           &buf, ctx->bn_ctx) != 33) {
            return "";
        }

        const std::string returning = base64_encode(buf, 33);

        OPENSSL_free(buf);

        return returning;
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPrivateKey() {
    if(key != NULL) {
        unsigned char *buf = (unsigned char*) malloc(sizeof(unsigned char) * 32);

        // TODO fix segfault here
        if (BN_bn2binpad(key->a, buf, 32) != 32) {
            return "";
        }

        const std::string returning = base64_encode(buf, 32);

        free(buf);

        return returning;
    } else {
        return "";
    }
}

bool CryptoKernel::Schnorr::setPublicKey(const std::string& publicKey) {
    const std::string decodedKey = base64_decode(publicKey);
    if(!EC_POINT_hex2point(ctx->group, (const char*)decodedKey.c_str(), key->pub->A, ctx->bn_ctx)) {
        return false;
    }
    return true;
}

bool CryptoKernel::Schnorr::setPrivateKey(const std::string& privateKey) {
    const std::string decodedKey = base64_decode(privateKey);
    if(!BN_hex2bn(&key->a, (const char*)decodedKey.c_str())){
        return false;
    }
    return true;
}


bool CryptoKernel::Schnorr::getStatus() {
    return true;
}
