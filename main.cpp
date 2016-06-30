#include <iostream>

#include "crypto.h"

int main()
{
    std::string hex1 = "1730758d11f43cbd78674c817a137d3a53402e00473c12b";
    std::string hex2 = "bf2863292ac429c033a4abffa95495fc21a4cfca39fa69";

    if(hex_greater(hex1, hex2))
    {
        std::cout << "Hex 1 is larger than hex 2\n";
    }

    std::cout << addHex(hex1, hex2) << std::endl;

    return 0;
}

