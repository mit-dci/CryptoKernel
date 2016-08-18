/*  CryptoKernel - A library for creating blockchain based digital currency
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
            Blockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime);
            ~Blockchain();
            struct output
            {
                std::string id;
                uint64_t value;
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
                bool mainChain;
            };
            bool submitTransaction(transaction tx);
            bool submitBlock(block newBlock, bool genesisBlock = false);
            uint64_t getBalance(std::string publicKey);
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
            std::vector<transaction> getUnconfirmedTransactions();

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
            bool reindexChain(std::string newTipId);
            std::string calculateTarget(std::string previousBlockId);
            virtual uint64_t getBlockReward(const uint64_t height) = 0;
            uint64_t getTransactionFee(transaction tx);
            uint64_t calculateTransactionFee(transaction tx);
            const std::string genesisBlockId = "85921dd96666c7bb649793ed582d81fe46f7ce112a7e412dec22c8ad82bbbab";
            bool status;
            bool reverseBlock();
            bool reorgChain(std::string newTipId);
            uint64_t blockTarget;
            virtual std::string PoWFunction(const std::string inputString) = 0;
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
