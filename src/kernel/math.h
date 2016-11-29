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
    /**
    * Static math module of CryptoKernel. Handles integer operations on
    * arbitrary length hex integers.
    */
    class Math
    {
        public:
            /**
            * Constructs an instance of the math class
            */
            Math();

            /**
            * Class deconstructor
            */
            ~Math();

            /**
            * Adds two hex strings and returns the result as a lower case
            * hex string.
            *
            * @param first the first integer to add
            * @param second the second integer to add
            * @return the sum of first and second as a lower case hex string
            */
            static std::string addHex(const std::string first, const std::string second);
            
            /**
            * Subtracts two hex integers from each other
            *
            * @param first the hex integer string to subtract from
            * @param second the hex integer string to subtract
            * @return the lowercase hex integer string from first - second
            */
            static std::string subtractHex(const std::string first, const std::string second);
            
            /**
            * Compares two hex strings to determine which is greater
            *
            * @param first the first hex integer to compare
            * @param second the second integer to check if greater than the first
            * @return true iff first greater than second, otherwise false
            */
            static bool hex_greater(const std::string first, const std::string second);
            
            /**
            * Divides two hex integer, one from the other
            *
            * @param first the numerator hex integer of the division
            * @param second the denominator hex integer of the division
            * @return the lowercase hex integer produced by first//second
            */
            static std::string divideHex(const std::string first, const std::string second);
            
            /**
            * Multiplies two hex integers
            * 
            * @param first the first integer to multiply
            * @param second the second integer to multiply
            * @return the lowercase hex integer produced by first * second
            */
            static std::string multiplyHex(const std::string first, const std::string second);
    };
}

#endif // MATH_H_INCLUDED
