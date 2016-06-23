#include <iostream>

#include "currency.h"

int main()
{
    CryptoKernel::Currency currency;

    std::cout << currency.getTotalCoins();

    return 0;
}

