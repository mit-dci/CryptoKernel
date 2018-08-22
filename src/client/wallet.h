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

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <random>
#include <iostream>

#include "storage.h"
#include "blockchain.h"
#include "network.h"
#include "crypto.h"

#define LATEST_WALLET_SCHEMA 2

namespace CryptoKernel {
class Wallet {
public:
    Wallet(CryptoKernel::Blockchain* blockchain,
           CryptoKernel::Network* network,
           CryptoKernel::Log* log,
           const std::string& dbDir);

    ~Wallet();

    class WalletException : public std::exception {
    public:
        WalletException(const std::string& message) {
            this->message = message;
        }

        virtual const char* what() const noexcept {
            return message.c_str();
        }

    private:
        std::string message;
    };

    class Account {
    public:
        Account(const std::string& name, const std::string& password);
        Account(const Json::Value& accountJson);

        Json::Value toJson() const;

        struct keyPair {
            std::string pubKey;
            std::shared_ptr<AES256> privKey;
            bool operator<(const keyPair& rhs) const {
                return pubKey < rhs.pubKey;
            }
        };

        std::string getName() const;

        std::set<keyPair> getKeys() const;

        keyPair newAddress(const std::string& password);

        bool operator<(const Account& rhs) const;

        void setBalance(const uint64_t newBalance);

        void addKeyPair(const keyPair& kp);

        uint64_t getBalance() const;

    private:
        std::set<keyPair> keys;
        std::string name;
        uint64_t balance;
    };

    class Txo {
    public:
        Txo(const std::string& id, const uint64_t value);
        Txo(const Json::Value& txoJson);

        Json::Value toJson() const;

        bool isSpent() const;
        void spend();

        uint64_t getValue() const;

    private:
        std::string id;
        bool spent;
        uint64_t value;
    };

    Account getAccountByName(const std::string& name);
    Account getAccountByKey(const std::string& pubKey);
    Account newAccount(const std::string& name, const std::string& password);

    Account importPrivKey(const std::string& name, const std::string& privKey,
                          const std::string& password);

    std::string sendToAddress(const std::string& pubKey,
                              const uint64_t amount,
                              const std::string& password);

    uint64_t getTotalBalance();

    std::set<Account> listAccounts();

    std::tuple<std::set<CryptoKernel::Blockchain::transaction>, std::set<CryptoKernel::Blockchain::transaction>> listTransactions();

    CryptoKernel::Blockchain::transaction signTransaction(const
            CryptoKernel::Blockchain::transaction& tx, const std::string& password);

    std::string signMessage(const std::string& message,
                            const std::string& publicKey,
                            const std::string& password);

private:
    std::unique_ptr<CryptoKernel::Storage> walletdb;
    std::unique_ptr<CryptoKernel::Storage::Table> accounts;
    std::unique_ptr<CryptoKernel::Storage::Table> utxos;
    std::unique_ptr<CryptoKernel::Storage::Table> transactions;
    std::unique_ptr<CryptoKernel::Storage::Table> params;

    CryptoKernel::Blockchain* blockchain;
    CryptoKernel::Network* network;
    CryptoKernel::Log* log;

    std::default_random_engine generator;

    std::unique_ptr<std::thread> watchThread;

    void watchFunc();
    bool running;

    void rewindBlock(CryptoKernel::Storage::Transaction* walletTx,
                     CryptoKernel::Storage::Transaction* bchainTx);

    void rewindTx(const CryptoKernel::Blockchain::transaction& tx,
                     CryptoKernel::Storage::Transaction* walletTx,
                     CryptoKernel::Storage::Transaction* bchainTx);

    void digestBlock(CryptoKernel::Storage::Transaction* walletTx,
                     CryptoKernel::Storage::Transaction* bchainTx,
                     const CryptoKernel::Blockchain::block& block);

    void digestTx(const CryptoKernel::Blockchain::transaction& tx,
                     CryptoKernel::Storage::Transaction* walletTx,
                     CryptoKernel::Storage::Transaction* bchainTx,
                     const bool unconfirmed = false);

    std::recursive_mutex walletLock;

    Account getAccountByKey(CryptoKernel::Storage::Transaction* dbTx,
                            const std::string& pubKey);

    void clearDB();

    uint64_t schemaVersion;

    void upgradeWallet();

    bool checkPassword(const std::string& password);
};
}

std::string getPass(const char *prompt, bool show_asterisk = true);

#ifdef __unix__
int getch();
#endif

#endif // WALLET_H_INCLUDED
