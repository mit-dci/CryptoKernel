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

#ifndef CRYPTO_H_INCLUDED
#define CRYPTO_H_INCLUDED

#include <string>
#include <memory>

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <json/value.h>

namespace CryptoKernel {

/**
* A mutable class which performs cryptography operations on ECDSA (secp256k1) keypairs. It provides funtions to
* generate keys, signing and verifying. It also provides a static function for generating SHA256 hashes.
*/
class Crypto {
public:

    /**
    * Constructs a crypto object. Optionally generates a secp256k1 keypair.
    *
    * @param fGenerate optional param, when set to true, generates a secp256k1 keypair. Otherwise if false or ommited, do not generate.
    */
    Crypto(const bool fGenerate = false);

    /**
    * Crypto class deconstructor
    */
    ~Crypto();

    /**
    * DEPRECATED: Returns the status of the crypto module
    *
    * @return the status of the crypto module. Always true.
    */
    bool getStatus();

    /**
    * Generates an ECDSA signature from the given message using a SHA256 digest.
    *
    * @param message the message to sign, must be non-empty
    * @return the ECDSA signature of the message with a SHA256 digest
    */
    std::string sign(std::string message);

    /**
    * Verifies a that a given signature verifies the given message with the public key of this class
    *
    * @param message the message to verify, must be non-empty
    * @param signature the signature to check against, must be non-empty
    * @return true if the signature is correctly verified against the message and public key, false otherwise
    */
    bool verify(std::string message, std::string signature);

    /**
    * Returns the public key of the instance
    *
    * @return the public key of the class encoded base64
    */
    std::string getPublicKey();

    /**
    * Returns the private key of the instance
    *
    * @return the private key of the class encoded base64
    */
    std::string getPrivateKey();

    /**
    * Sets the public key of the instance
    *
    * @param publicKey valid public key from another instance of Crypto
    * @return true if setting the key was successful, false otherwise
    */
    bool setPublicKey(std::string publicKey);

    /**
    * Sets the private key of the instance
    *
    * @param privateKey valid private key from another instance of Crypto
    * @return true if setting the key was successful, false otherwise
    */
    bool setPrivateKey(std::string privateKey);

    /**
    * Static function which generates a hex encoded SHA256 hash of the given message
    *
    * @param message the message to hash, must be non-empty
    * @return the SHA256 hash of the message, hex encoded, lowercase
    */
    static std::string sha256(std::string message);

private:
    EC_KEY *eckey;
    EC_GROUP *ecgroup;
};


class AES256 {
    public:
        AES256(const Json::Value& objJson);
        AES256(const std::string& password, const std::string& plainText);

        Json::Value toJson() const;

        std::string decrypt(const std::string& password) const;

    private:
        unsigned char salt[32];
        unsigned char iv[16];
        std::string cipherText;

        std::shared_ptr<unsigned char> genKey(const std::string& password) const;
};

}

std::string base16_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

#endif // CRYPTO_H_INCLUDED
