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
