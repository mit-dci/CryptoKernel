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

#include <openssl/bn.h>

#include "ckmath.h"

std::string CryptoKernel::Math::addHex(std::string first, std::string second)
{
    if(first.size() > 0 && second.size() > 0)
    {
        BIGNUM *bn1 = NULL;
        BIGNUM *bn2 = NULL;

        BN_CTX *ctx = BN_CTX_new();

        BN_hex2bn(&bn1, first.c_str());
        BN_hex2bn(&bn2, second.c_str());

        BN_add(bn1, bn1, bn2);

        std::stringstream buffer;
        buffer << BN_bn2hex(bn1);

        BN_free(bn1);
        BN_free(bn2);
        BN_CTX_free(ctx);

        std::string returning = buffer.str();
        std::transform(returning.begin(), returning.end(), returning.begin(), ::tolower);

        return returning;
    }
    else if(first.size() > 0)
    {
        std::transform(first.begin(), first.end(), first.begin(), ::tolower);
        return first;
    }
    else if(second.size() > 0)
    {
        std::transform(second.begin(), second.end(), second.begin(), ::tolower);
        return second;
    }
    else
    {
        return "0";
    }
}

std::string CryptoKernel::Math::subtractHex(std::string first, std::string second)
{
    if(first.size() > 0 && second.size() > 0)
    {
        BIGNUM *bn1 = NULL;
        BIGNUM *bn2 = NULL;

        BN_CTX *ctx = BN_CTX_new();

        BN_hex2bn(&bn1, first.c_str());
        BN_hex2bn(&bn2, second.c_str());

        BN_sub(bn1, bn1, bn2);

        std::stringstream buffer;
        buffer << BN_bn2hex(bn1);

        BN_free(bn1);
        BN_free(bn2);
        BN_CTX_free(ctx);

        std::string returning = buffer.str();
        std::transform(returning.begin(), returning.end(), returning.begin(), ::tolower);

        return returning;
    }
    else if(first.size() > 0)
    {
        std::transform(first.begin(), first.end(), first.begin(), ::tolower);
        return first;
    }
    else
    {
        return "0";
    }
}

bool CryptoKernel::Math::hex_greater(std::string first, std::string second)
{
    if(first.size() > 0 && second.size() > 0)
    {
        BIGNUM *bn1 = NULL;
        BIGNUM *bn2 = NULL;

        BN_CTX *ctx = BN_CTX_new();

        BN_hex2bn(&bn1, first.c_str());
        BN_hex2bn(&bn2, second.c_str());

        const int test = BN_cmp(bn1, bn2);

        BN_free(bn1);
        BN_free(bn2);
        BN_CTX_free(ctx);

        if(test > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if(first.size() > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::string CryptoKernel::Math::divideHex(std::string first, std::string second)
{
    BIGNUM *bn1 = NULL;
    BIGNUM *bn2 = NULL;

    BN_CTX *ctx = BN_CTX_new();

    BN_hex2bn(&bn1, first.c_str());
    BN_hex2bn(&bn2, second.c_str());

    BIGNUM *DivBN = BN_new();
    BIGNUM *RemBN = BN_new();
    BN_div(DivBN, RemBN, bn1, bn2, ctx);

    std::stringstream buffer;
    buffer << BN_bn2hex(DivBN);

    BN_free(bn1);
    BN_free(bn2);
    BN_free(DivBN);
    BN_free(RemBN);
    BN_CTX_free(ctx);

    std::string returning = buffer.str();
    std::transform(returning.begin(), returning.end(), returning.begin(), ::tolower);

    return returning;
}

std::string CryptoKernel::Math::multiplyHex(std::string first, std::string second)
{
    BIGNUM *bn1 = NULL;
    BIGNUM *bn2 = NULL;

    BN_CTX *ctx = BN_CTX_new();

    BN_hex2bn(&bn1, first.c_str());
    BN_hex2bn(&bn2, second.c_str());

    BIGNUM *MulBN = BN_new();
    BN_mul(MulBN, bn1, bn2, ctx);

    std::stringstream buffer;
    buffer << BN_bn2hex(MulBN);

    BN_free(bn1);
    BN_free(bn2);
    BN_free(MulBN);
    BN_CTX_free(ctx);

    std::string returning = buffer.str();
    std::transform(returning.begin(), returning.end(), returning.begin(), ::tolower);

    return returning;
}
