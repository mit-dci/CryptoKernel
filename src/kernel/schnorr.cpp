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
    pkey = key->pub;

    if(key == NULL) {
        throw std::runtime_error("Could not generate key pair");
    }
}

CryptoKernel::Schnorr::~Schnorr() {
    schnorr_context_free(ctx);
    musig_key_free(key);
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
        musig_sig_free(sig);
        return false;
    }

    sig->R = EC_POINT_new(ctx->group);
    if (sig->R == NULL) {
        musig_sig_free(sig);
        return false;
    }

    if (!BN_bin2bn((unsigned char*)decodedSignature.c_str(), 32, sig->s)) {
        musig_sig_free(sig);
        return false;
    }

    if (!EC_POINT_oct2point(
        ctx->group,
        sig->R,
        (unsigned char*)decodedSignature.c_str() + 32,
        33,
        ctx->bn_ctx)) {
        musig_sig_free(sig);
        return false;
    }

    if (musig_verify(
        ctx,
        sig,
        pkey,
        (unsigned char*)message.c_str(),
        message.size()) != 1) {
        musig_sig_free(sig);
        return false;
    }

    musig_sig_free(sig);

    return true;
}

std::string CryptoKernel::Schnorr::signSingle(const std::string& message) {
    if (key != NULL) {
        musig_sig* sig;

        if (musig_sign_single(
            ctx,
            &sig,
            key,
            (unsigned char*)message.c_str(),
            message.size()) == 0) {
            musig_sig_free(sig);

            throw std::runtime_error("Could not sign message");
        } else {
            const unsigned int buf_len = 65;
            std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

            if (BN_bn2binpad(sig->s, buf.get(), 32) != 32) {
                musig_sig_free(sig);
                throw std::runtime_error("Failed to encode s");
            }

            if (EC_POINT_point2oct(
                    ctx->group,
                    sig->R,
                    POINT_CONVERSION_COMPRESSED,
                    buf.get() + 32,
                    33,
                    ctx->bn_ctx) != 33) {
                musig_sig_free(sig);
                throw std::runtime_error("Failed to encode R");
            }

            const std::string returning = base64_encode(buf.get(), buf_len);

            musig_sig_free(sig);

            return returning;
        }
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::pubkeyAggregate(const std::set<std::string>& pubkeys) {
    std::unique_ptr<musig_pubkey> keys[pubkeys.size()];

    unsigned int i = 0;
    for(const auto& p : pubkeys) {
        const std::string decodedKey = base64_decode(p);

        keys[i].reset(new musig_pubkey);

        keys[i]->A = EC_POINT_new(ctx->group);

        if(!EC_POINT_oct2point(
                ctx->group,
                keys[i]->A,
                (unsigned char*)decodedKey.c_str(),
                decodedKey.size(),
                ctx->bn_ctx)) {
            return "";
        }

        i++;
    }

    musig_pubkey* keysArr[pubkeys.size()];
    i = 0;
    for(const auto& p : keys) {
        keysArr[i] = p.get();
        i++;
    }

    musig_pubkey* pk;
    if(musig_pubkey_aggregate(ctx, keysArr, &pk, pubkeys.size()) != 1) {
        return "";
    }

    const unsigned int buf_len = 33;
    std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

    if (EC_POINT_point2oct(
            ctx->group,
            pk->A,
            POINT_CONVERSION_COMPRESSED,
            buf.get(),
            buf_len,
            ctx->bn_ctx) != buf_len) {
        return "";
    }

    musig_pubkey_free(pk);

    const std::string returning = base64_encode(buf.get(), buf_len);

    return returning;
}

std::string CryptoKernel::Schnorr::getPublicKey() {
    if (key != NULL) {
        const unsigned int buf_len = 33;
        std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

        if (EC_POINT_point2oct(
                ctx->group,
                pkey->A,
                POINT_CONVERSION_COMPRESSED,
                buf.get(),
                buf_len,
                ctx->bn_ctx) != buf_len) {
            return "";
        }

        const std::string returning = base64_encode(buf.get(), buf_len);

        return returning;
    } else {
        return "";
    }
}

std::string CryptoKernel::Schnorr::getPrivateKey() {
    if (key != NULL) {
        const unsigned int buf_len = 32;
        std::unique_ptr<unsigned char> buf(new unsigned char[buf_len]);

        if (BN_bn2binpad(key->a, buf.get(), buf_len) != buf_len) {
            return "";
        }

        const std::string returning = base64_encode(buf.get(), buf_len);

        return returning;
    } else {
        return "";
    }
}

bool CryptoKernel::Schnorr::setPublicKey(const std::string& publicKey) {
    const std::string decodedKey = base64_decode(publicKey);

    if(!EC_POINT_oct2point(
            ctx->group,
            pkey->A,
            (unsigned char*)decodedKey.c_str(),
            decodedKey.size(),
            ctx->bn_ctx)) {
        return false;
    }

    return true;
}

bool CryptoKernel::Schnorr::setPrivateKey(const std::string& privateKey) {
    const std::string decodedKey = base64_decode(privateKey);

    if(!BN_bin2bn(
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
