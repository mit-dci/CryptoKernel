#include <iostream>

#include "crypto.h"

int main()
{
    std::string hex1 = "17cdbf";
    std::string hex2 = "8000fa";

    std::cout << subtractHex(hex2, hex1) << std::endl;

    return 0;
}

