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

namespace CryptoKernel
{
    class Math
    {
        public:
            Math();
            ~Math();
            static std::string addHex(std::string first, std::string second);
            static std::string subtractHex(std::string first, std::string second);
            static bool hex_greater(std::string first, std::string second);
            static std::string divideHex(std::string first, std::string second);
            static std::string multiplyHex(std::string first, std::string second);
    };
}

#endif // MATH_H_INCLUDED
