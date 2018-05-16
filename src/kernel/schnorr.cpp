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

#include "schnorr.h"
#include "base64.h"

CryptoKernel::Schnorr::Schnorr() {
    ctx = schnorr_context_new();
    key = musig_key_new(ctx);

    if (key == NULL) {
        throw std::runtime_error("Could not generate key pair");
    }
}

CryptoKernel::Schnorr::~Schnorr() {
    schnorr_context_free(ctx);
    // TODO(metalicjames): musig_key_free(key);
}

bool CryptoKernel::Schnorr::verify(const std::string& message,
                                   const std::string& signature) {
    const std::string decodedSignature = base64_decode(signature);

    musig_sig* sig = NULL;
    sig = reinterpret_cast<musig_sig*>(malloc(sizeof(musig_sig)));
    if (sig == NULL) {
        return false;
    }
    sig->s = NULL;
    sig->R = NULL;

    sig->s = BN_new();
    if (sig->s == NULL) {
        return false;
    }

    sig->R = EC_POINT_new(ctx->group);
    if (sig->R == NULL) {
        return false;
    }

    if (!BN_bin2bn((unsigned char*)decodedSignature.c_str(), 32, sig->s)) {
        return false;
    }

    if (!EC_POINT_oct2point(
        ctx->group,
        sig->R,
        (unsigned char*)decodedSignature.c_str() + 32,
        33,
        ctx->bn_ctx)) {
        return false;
    }

    if (musig_verify(
        ctx,
        sig,
        key->pub,
        (unsigned char*)message.c_str(),
        message.length()) != 1) {
        return false;
    }

    return true;
}

std::string CryptoKernel::Schnorr::sign(const std::string& message) {
    if (key != NULL) {
        musig_sig* sig;
        musig_pubkey* pubkeys[1];
        pubkeys[0] = key->pub;

        musig_pubkey* pub;

        if (musig_sign(
            ctx,
            &sig,
            &pub,
            key,
            pubkeys,
            1,
            (unsigned char*)message.c_str(),
            message.size()) == 0) {
            delete[] sig;

            throw std::runtime_error("Could not sign message");
        } else {
            unsigned char buf[65];

            if (BN_bn2binpad(sig->s, (unsigned char*)&buf, 32) != 32) {
                throw std::runtime_error("Failed to encode s");
            }

            if (EC_POINT_point2oct(
                    ctx->group,
                    sig->R,
                    POINT_CONVERSION_COMPRESSED,
                    (unsigned char*)&buf + 32,
                    33,
                    ctx->bn_ctx) != 33) {
                throw std::runtime_error("Failed to encode R");
            }

            const std::string returning = base64_encode(buf, 65);

            // delete[] buf;
            delete[] sig;

            return returning;
        }
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPublicKey() {
    if (key != NULL) {
        unsigned int buf_len = 33;
        unsigned char *buf;
        buf = new unsigned char[buf_len];

        if (EC_POINT_point2oct(
                ctx->group,
                key->pub->A,
                POINT_CONVERSION_COMPRESSED,
                buf,
                buf_len,
                ctx->bn_ctx) != buf_len) {
            return "";
        }

        const std::string returning = base64_encode(buf, buf_len);

        delete[] buf;

        return returning;
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPrivateKey() {
    if (key != NULL) {
        int buf_len = 32;
        unsigned char *buf;
        buf = new unsigned char[buf_len];

        if (BN_bn2binpad(key->a, buf, buf_len) != buf_len) {
            return "";
        }

        const std::string returning = base64_encode(buf, buf_len);

        delete[] buf;

        return returning;
    } else {
        return "";
    }
}

bool CryptoKernel::Schnorr::setPublicKey(const std::string& publicKey) {
    const std::string decodedKey = base64_decode(publicKey);

    if (!EC_POINT_oct2point(
            ctx->group,
            key->pub->A,
            (unsigned char*)decodedKey.c_str(),
            decodedKey.size(),
            ctx->bn_ctx)) {
        return false;
    }
    return true;
}

bool CryptoKernel::Schnorr::setPrivateKey(const std::string& privateKey) {
    const std::string decodedKey = base64_decode(privateKey);

    if (!BN_bin2bn(
            (unsigned char*)decodedKey.c_str(),
            (unsigned int)decodedKey.size(),
            key->a)) {
        return false;
    }
    return true;
}


bool CryptoKernel::Schnorr::getStatus() {
    return true;
}
