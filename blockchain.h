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
                uint64_t timestamp;
            };
            struct block
            {
                std::string id;
                unsigned int height;
                std::vector<transaction> transactions;
                std::string previousBlockId;
                uint64_t timestamp;
                std::string target;
                std::string PoW;
                std::string totalWork;
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
            block jsonToBlock(Json::Value Block);
            transaction jsonToTransaction(Json::Value tx);
            output jsonToOutput(Json::Value Output);
            std::string calculateOutputId(output Output);
            std::string calculateTransactionId(transaction tx);
            std::string calculateBlockId(block Block);
            bool verifyTransaction(transaction tx);
            bool confirmTransaction(transaction tx);
            std::string chainTipId;
            bool reorgChain(std::string newTipId);
            std::string calculateTarget(std::string previousBlockId);
            double getBlockReward();
            bool isCoinbaseTransaction(transaction tx);
            double getTransactionFee(transaction tx);
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
