/*
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

#ifndef SCHNORR_H_INCLUDED
#define SCHNORR_H_INCLUDED

#include <string>
#include <memory>
#include <set>

#include <cschnorr/multisig.h>
#include <json/value.h>


namespace CryptoKernel {

/**
* A mutable class which performs cryptography operations on Schnorr keypairs. It provides funtions to
* generate keys, signing and verifying.
*/
class Schnorr {
public:
    /**
    * Constructs a Schnorr object. Optionally generates a Schnorr keypair.
    */
    Schnorr();

    /**
    * Schnorr class deconstructor
    */
    ~Schnorr();

    /**
    * DEPRECATED: Returns the status of the Schnorr module
    *
    * @return the status of the Schnorr module. Always true.
    */
    bool getStatus();

    /**
    * Generates an Schnorr signature from the given message.
    *
    * @param message the message to sign, must be non-empty
    * @return the Schnorr signature of the message and public key as base64_encode(s + R)
    */
    std::string signSingle(const std::string& message);

    /**
    * Verifies a that a given signature verifies the given message with the public key of this class
    *
    * @param message the message to verify, must be non-empty
    * @param signature the signature to check against, must be non-empty
    * @return true if the signature is correctly verified against the message and public key, false otherwise
    */
    bool verify(const std::string& message, const std::string& signature);

    /**
     * Aggregates a set of public keys in to an aggregate public key.
     *
     * @param pubkeys a set of pubkeys to be aggregated
     * @return the aggregated pubkey if successful, empty string otherwise
     */
    std::string pubkeyAggregate(const std::set<std::string>& pubkeys);

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
    * @param publicKey valid public key from another instance of Schnorr
    * @return true if setting the key was successful, false otherwise
    */
    bool setPublicKey(const std::string& publicKey);

    /**
    * Sets the private key of the instance
    *
    * @param privateKey valid private key from another instance of Schnorr
    * @return true if setting the key was successful, false otherwise
    */
    bool setPrivateKey(const std::string& privateKey);

private:
    musig_key* key;
    musig_pubkey* pkey;
    schnorr_context* ctx;
};

}

#endif  // SCHNORR_H_INCLUDED
