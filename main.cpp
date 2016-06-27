#include <iostream>

#include "currency.h"

int main()
{
    CryptoKernel::Currency currency;

    std::cout << currency.getBalance("jamesl22") << std::endl;

    std::cout << currency.getBalance("jamesl22test") << std::endl;

    currency.moveCoins("jamesl22", "jamesl22test", 10.5, 0.01);

    std::cout << currency.getBalance("jamesl22") << std::endl;

    std::cout << currency.getBalance("jamesl22test") << std::endl;

    std::cout << currency.getTotalCoins();

    return 0;
}

