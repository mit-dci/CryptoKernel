#ifndef CURRENCY_H_INCLUDED
#define CURRENCY_H_INCLUDED

#include "storage.h"

namespace CryptoKernel
{
    class Currency
    {
        public:
            Currency();
            ~Currency();
            double getBalance(std::string accountName);
            bool mintCoins(std::string accountName, double amount);
            bool moveCoins(std::string accountFrom, std::string accountTo, double amount, double fee);
            bool newAccount(std::string accountName);
            double getTotalCoins();
            bool accountExists(std::string accountName);

        private:
            struct account
            {
                std::string name;
                double balance;
            };
            Storage* accounts;
            account jsonToAccount(Json::Value accountJson);
            Json::Value accountToJson(account accountStruct);

    };
}

#endif // CURRENCY_H_INCLUDED
