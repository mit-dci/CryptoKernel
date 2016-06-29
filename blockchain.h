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
            bool submitBlock(block newBlock);
            double getBalance(std::string publicKey);


        private:
            Storage *transactions;
            Storage *blocks;
            Storage *utxos;
            std::vector<transaction> unconfirmedTransactions;
            Json::Value transactionToJson(transaction tx);
            Json::Value outputToJson(output Output);
            Json::Value blockToJson(block Block);
            std::string calculateOutputId(output Output);
            std::string calculateTransactionId(transaction tx);
            std::string calculateBlockId(block Block);
            bool verifyTransaction(transaction tx);
            bool confirmTransaction(transaction tx);
            std::string chainTipId;
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
