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

#ifndef MATH_H_INCLUDED
#define MATH_H_INCLUDED

#include <string>

#include <openssl/bn.h>

namespace CryptoKernel {
class BigNum {
public:
    BigNum(const std::string& hexString);

    BigNum();

    BigNum(const BigNum& other);

    ~BigNum();

    std::string toString() const;

    void operator=(const BigNum& other);

    BigNum operator+(const BigNum& rhs) const;
    BigNum operator-(const BigNum& rhs) const;
    BigNum operator*(const BigNum& rhs) const;
    BigNum operator/(const BigNum& rhs) const;

    bool operator!=(const BigNum& rhs) const;
    bool operator==(const BigNum& rhs) const;

    bool operator>(const BigNum& rhs) const;
    bool operator<(const BigNum& rhs) const;
    bool operator>=(const BigNum& rhs) const;
    bool operator<=(const BigNum& rhs) const;

private:
    BIGNUM* bn;

    int compare(const BigNum& lhs, const BigNum& rhs) const;
};
}

#endif // MATH_H_INCLUDED
