#ifndef BLOCKCHAIN_H_INCLUDED
#define BLOCKCHAIN_H_INCLUDED

#include <vector>
#include <queue>

#include "currency.h"

namespace CryptoKernel
{
    class Blockchain
    {
        public:
            Blockchain();
            ~Blockchain();
            struct output
            {
                std::string id;
                double value;
                std::string signature;
                std::string publicKey;
                Json::Value data;
            };
            struct transaction
            {
                std::string id;
                std::vector<output> inputs;
                std::vector<output> outputs;
                unsigned int timestamp;
            };
            struct block
            {
                std::string id;
                unsigned int height;
                std::vector<transaction> transactions;
                std::string previousBlockId;
                unsigned int timestamp;
            };
            bool submitTransaction(transaction tx);
            std::vector<transaction> getUnconfirmedTransactions();
            bool submitBlock(block newBlock);
            double getBalance(std::string publicKey);


        private:
            Storage *transactions;
            Storage *blocks;
            Storage *utxos;
            Currency *currency;
            std::queue<transaction> unconfirmedTransactions;
            Json::Value transactionToJson(transaction tx);
            Json::Value outputToJson(output Output);
            std::string calculateOutputId(output Output);
            std::string calculateTransactionId(transaction tx);
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
