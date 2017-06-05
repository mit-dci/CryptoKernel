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

#include "ckmath.h"

CryptoKernel::BigNum::BigNum(const std::string& hexString) {
    bn = BN_new();
    BN_hex2bn(&bn, hexString.c_str());
}

CryptoKernel::BigNum::BigNum() {
    bn = BN_new();
}

CryptoKernel::BigNum::BigNum(const BigNum& other) {
    bn = BN_new();
    BN_copy(bn, other.bn);
}

CryptoKernel::BigNum::~BigNum() {
    BN_free(bn);
}

std::string CryptoKernel::BigNum::toString() const {
    std::stringstream buffer;

    char* hexStr = BN_bn2hex(bn);

    buffer << hexStr;

    free(hexStr);

    std::string returning = buffer.str();
    std::transform(returning.begin(), returning.end(), returning.begin(), ::tolower);

    returning.erase(0, std::min(returning.find_first_not_of('0'), returning.size()-1));

    return returning;
}

void CryptoKernel::BigNum::operator=(const BigNum& other) {
    BN_copy(bn, other.bn);
}

CryptoKernel::BigNum CryptoKernel::BigNum::operator+(const BigNum& rhs) const {
    BigNum returning;
    BN_add(returning.bn, bn, rhs.bn);
    return returning;
}

CryptoKernel::BigNum CryptoKernel::BigNum::operator-(const BigNum& rhs) const {
    BigNum returning;
    BN_sub(returning.bn, bn, rhs.bn);
    return returning;
}

CryptoKernel::BigNum CryptoKernel::BigNum::operator*(const BigNum& rhs) const {
    BigNum returning;
    BN_CTX *ctx = BN_CTX_new();
    BN_mul(returning.bn, bn, rhs.bn, ctx);
    BN_CTX_free(ctx);
    return returning;
}

CryptoKernel::BigNum CryptoKernel::BigNum::operator/(const BigNum& rhs) const {
    BigNum returning;
    BN_CTX *ctx = BN_CTX_new();
    BIGNUM *remBN = BN_new();
    BN_div(returning.bn, remBN, bn, rhs.bn, ctx);
    BN_free(remBN);
    BN_CTX_free(ctx);
    return returning;
}

int CryptoKernel::BigNum::compare(const BigNum& lhs, const BigNum& rhs) const {
    return BN_cmp(lhs.bn, rhs.bn);
}

bool CryptoKernel::BigNum::operator==(const BigNum& rhs) const {
    return compare(*this, rhs) == 0;
}

bool CryptoKernel::BigNum::operator!=(const BigNum& rhs) const {
    return compare(*this, rhs) != 0;
}

bool CryptoKernel::BigNum::operator>(const BigNum& rhs) const {
    return compare(*this, rhs) > 0;
}

bool CryptoKernel::BigNum::operator<(const BigNum& rhs) const {
    return compare(*this, rhs) < 0;
}

bool CryptoKernel::BigNum::operator>=(const BigNum& rhs) const {
    return compare(*this, rhs) >= 0;
}

bool CryptoKernel::BigNum::operator<=(const BigNum& rhs) const {
    return compare(*this, rhs) <= 0;
}
