#ifndef BLOCKCHAIN_H_INCLUDED
#define BLOCKCHAIN_H_INCLUDED

#include <vector>

#include "storage.h"
#include "log.h"

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
                uint64_t nonce;
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
                transaction coinbaseTx;
                std::string previousBlockId;
                uint64_t timestamp;
                std::string target;
                std::string PoW;
                std::string totalWork;
                uint64_t nonce;
            };
            bool submitTransaction(transaction tx);
            bool submitBlock(block newBlock, bool genesisBlock = false);
            double getBalance(std::string publicKey);
            block generateMiningBlock(std::string publicKey);
            Json::Value transactionToJson(transaction tx);
            Json::Value outputToJson(output Output);
            Json::Value blockToJson(block Block, bool PoW = false);
            block jsonToBlock(Json::Value Block);
            transaction jsonToTransaction(Json::Value tx);
            output jsonToOutput(Json::Value Output);
            std::string calculatePoW(block Block);
            block getBlock(std::string id);
            std::vector<output> getUnspentOutputs(std::string publicKey);
            std::string calculateOutputId(output Output);
            std::string calculateTransactionId(transaction tx);
            std::string calculateOutputSetId(std::vector<output> outputs);

        private:
            Storage *transactions;
            Storage *blocks;
            Storage *utxos;
            Log *log;
            std::vector<transaction> unconfirmedTransactions;
            std::string calculateBlockId(block Block);
            bool verifyTransaction(transaction tx, bool coinbaseTx = false);
            bool confirmTransaction(transaction tx, bool coinbaseTx = false);
            std::string chainTipId;
            bool reorgChain(std::string newTipId);
            std::string calculateTarget(std::string previousBlockId);
            double getBlockReward();
            double getTransactionFee(transaction tx);
            double calculateTransactionFee(transaction tx);
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
