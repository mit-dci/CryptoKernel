#include <sstream>
#include <algorithm>

#include <openssl/bn.h>

#include "math.h"

std::string CryptoKernel::Math::addHex(std::string first, std::string second)
{
    BIGNUM *bn1 = NULL;
    BIGNUM *bn2 = NULL;

    BN_CTX *ctx = BN_CTX_new();

    if(first.size() > 0 && second.size() > 0)
    {
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
        return first;
    }
    else if(second.size() > 0)
    {
        return second;
    }
    else
    {
        return "0";
    }
}

std::string CryptoKernel::Math::subtractHex(std::string first, std::string second)
{
    BIGNUM *bn1 = NULL;
    BIGNUM *bn2 = NULL;

    BN_CTX *ctx = BN_CTX_new();

    if(first.size() > 0 && second.size() > 0)
    {
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
        return first;
    }
    else
    {
        return "0";
    }
}

bool CryptoKernel::Math::hex_greater(std::string first, std::string second)
{
    BIGNUM *bn1 = NULL;
    BIGNUM *bn2 = NULL;

    BN_CTX *ctx = BN_CTX_new();

    if(first.size() > 0 && second.size() > 0)
    {
        BN_hex2bn(&bn1, first.c_str());
        BN_hex2bn(&bn2, second.c_str());

        int test = BN_cmp(bn1, bn2);

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
