#include "currency.h"

#include <iostream>

CryptoKernel::Currency::Currency()
{
    accounts = new CryptoKernel::Storage("./accountsdb");
}

CryptoKernel::Currency::~Currency()
{
    delete accounts;
}

double CryptoKernel::Currency::getTotalCoins()
{
    CryptoKernel::Storage::Iterator* it = accounts->newIterator();

    double total;
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        account Account = jsonToAccount(it->value());
        total += Account.balance;
    }

    delete it;

    return total;
}

bool CryptoKernel::Currency::newAccount(std::string accountName)
{
    if(!accountExists(accountName) && accountName != "")
    {
        account newaccount;
        newaccount.name = accountName;
        newaccount.balance = 0;

        accounts->store(accountName, accountToJson(newaccount));

        return true;
    }
    else
    {
        return false;
    }
}

bool CryptoKernel::Currency::moveCoins(std::string accountFrom, std::string accountTo, double amount, double fee)
{
    if(accountExists(accountFrom) && accountExists(accountTo) && amount >= 0.00000001 && (fee >= 0.00000001 || fee == 0) && accountFrom != accountTo)
    {
        account sender = jsonToAccount(accounts->get(accountFrom));
        if(sender.balance >= (amount + fee))
        {
            sender.balance -= (amount + fee);
            accounts->store(accountFrom, accountToJson(sender));

            account recipient = jsonToAccount(accounts->get(accountTo));
            recipient.balance += amount;
            accounts->store(accountTo, accountToJson(recipient));

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

double CryptoKernel::Currency::getBalance(std::string accountName)
{
    if(accountExists(accountName))
    {
        account Account = jsonToAccount(accounts->get(accountName));
        return Account.balance;
    }
    else
    {
        return 0;
    }
}

bool CryptoKernel::Currency::mintCoins(std::string accountName, double amount)
{
    if(accountExists(accountName) && amount >= 0.00000001)
    {
        account Account = jsonToAccount(accounts->get(accountName));
        Account.balance += amount;
        accounts->store(accountName, accountToJson(Account));

        return true;
    }
    else
    {
        return false;
    }
}

bool CryptoKernel::Currency::accountExists(std::string accountName)
{
    Json::Value accountJson = accounts->get(accountName);

    return (accountJson["name"] != "");
}

CryptoKernel::Currency::account CryptoKernel::Currency::jsonToAccount(Json::Value accountJson)
{
    account returning;

    returning.name = accountJson["name"].asString();
    returning.balance = accountJson["balance"].asDouble();

    return returning;
}

Json::Value CryptoKernel::Currency::accountToJson(account accountStruct)
{
    Json::Value returning;

    returning["name"] = accountStruct.name;
    returning["balance"] = accountStruct.balance;

    return returning;
}
