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
