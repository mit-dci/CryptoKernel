/*  CryptoCurrency - An example crypto-currency using the CryptoKernel library as a base
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

#ifndef WALLET_H_INCLUDED
#define WALLET_H_INCLUDED

#include "storage.h"
#include "log.h"
#include "blockchain.h"
#include "network.h"

namespace CryptoCurrency
{
    class Wallet
    {
        public:
            Wallet(CryptoKernel::Blockchain* Blockchain, CryptoKernel::Network* network);
            ~Wallet();
            struct address
            {
                std::string name;
                std::string publicKey;
                std::string privateKey;
                uint64_t balance;
            };
            address newAddress(std::string name);
            address getAddressByName(std::string name);
            address getAddressByKey(std::string publicKey);
            bool sendToAddress(const std::string& publicKey, const uint64_t amount, const uint64_t fee);
            double getTotalBalance();
            void rescan();

            std::vector<address> listAddresses();
            Json::Value addressToJson(address Address);
            address jsonToAddress(Json::Value Address);
            std::set<CryptoKernel::Blockchain::transaction> listTransactions();

            CryptoKernel::Blockchain::transaction signTransaction(const CryptoKernel::Blockchain::transaction tx);

        private:
            std::unique_ptr<CryptoKernel::Storage> walletdb;
            std::unique_ptr<CryptoKernel::Storage::Table> addresses;

            bool updateAddressBalance(CryptoKernel::Storage::Transaction* dbTx, std::string name, uint64_t amount);

            CryptoKernel::Blockchain* blockchain;
            CryptoKernel::Network* network;

            std::default_random_engine generator;
    };
}

#endif // WALLET_H_INCLUDED
