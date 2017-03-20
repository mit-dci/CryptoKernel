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
#include <set>

#include "storage.h"
#include "log.h"
#include "ckmath.h"

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
                Json::Value spendData;
                std::string creationTx;
            };
            struct transaction
            {
                std::string id;
                std::vector<output> inputs;
                std::vector<output> outputs;
                uint64_t timestamp;
                std::string confirmingBlock;
                friend bool operator<(const transaction& lhs, const transaction& rhs) {
                    return CryptoKernel::Math::hex_greater(rhs.id, lhs.id);
                };
            };
            struct block
            {
                std::string id;
                unsigned int height;
                std::vector<transaction> transactions;
                transaction coinbaseTx;
                std::string previousBlockId;
                std::string publicKey;
                std::string signature;
                uint64_t timestamp;
                bool mainChain;
                uint64_t sequenceNumber;
            };
            bool submitTransaction(transaction tx);
            bool submitBlock(block newBlock, bool genesisBlock = false);
            uint64_t getBalance(std::string publicKey);
            block generateMiningBlock(std::string publicKey);
            static Json::Value transactionToJson(transaction tx);
            static Json::Value outputToJson(output Output);
            static Json::Value blockToJson(block Block, bool PoW = false);
            static block jsonToBlock(Json::Value Block);
            static transaction jsonToTransaction(Json::Value tx);
            static output jsonToOutput(Json::Value Output);
            block getBlock(std::string id);

            /**
            * Retrieves the block from the current main chain with the given height
            *
            * @param height the height of the block to retrieve
            * @return the block with the given height in the main chain, or an empty
            *         block if it does not exist
            */
            block getBlockByHeight(const uint64_t height);

            /**
            * Retrieves the transaction with the given id
            *
            * @param id the id of the transaction to get
            * @return the confirmed transaction with the given id or an empty transaction
            *         if it doesn't exist
            */
            transaction getTransaction(const std::string id);

            /**
            * Retrieves all of the transactions associated with
            * a given set of public keys by checking the publicKey
            * field in every transactions' inputs and outputs.
            *
            * @param pubKeys a set containing the public keys to search for
            * @return a set containing the transactions associated with the key
            *         set
            */
            std::set<transaction> getTransactionsByPubKeys(const std::set<std::string> pubKeys);

            std::vector<output> getUnspentOutputs(std::string publicKey);
            static std::string calculateOutputId(output Output);
            static std::string calculateTransactionId(transaction tx);
            static std::string calculateOutputSetId(const std::vector<output> outputs);
            std::vector<transaction> getUnconfirmedTransactions();
            bool loadChain();
            Storage::Iterator* newIterator();
            const std::string genesisBlockId = "acc69da369fbac099bbb9ae38a637eec4f27358e9874828964d02ee8bb91cd38";
            std::string getVerifier(const block& thisBlock);

        private:
            Storage *transactions;
            Storage *blocks;
            Storage *utxos;
            Log *log;
            void checkRep();
            std::vector<transaction> unconfirmedTransactions;
            std::string calculateBlockId(block Block);
            bool verifyTransaction(transaction tx, bool coinbaseTx = false);
            bool confirmTransaction(transaction tx, bool coinbaseTx = false);
            std::string chainTipId;
            bool reindexChain(std::string newTipId);
            uint64_t getTransactionFee(transaction tx);
            uint64_t calculateTransactionFee(transaction tx);
            bool status;
            bool reverseBlock();
            bool reorgChain(std::string newTipId);
            uint64_t blockTarget;
            std::recursive_mutex chainLock;
            std::set<std::string> verifiers;
            const std::string cbKey = "BLNz4IiBnUDanMovX5LQ9KCev1bUVO/70r0WqXv2Gc96SnsPuayoXXYlIivrQ9C8vhOm7scoXm3QXMgid2vsvEs=";
            virtual uint64_t getBlockReward(const uint64_t height) = 0;
    };
}

#endif // BLOCKCHAIN_H_INCLUDED
