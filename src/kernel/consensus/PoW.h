#ifndef POW_H_INCLUDED
#define POW_H_INCLUDED

#include "blockchain.h"

namespace CryptoKernel {
    /**
    * Implements a Proof of Work consensus algorithm that is very similar to Bitcoin
    * in semantics.
    */
    class Consensus::PoW : public Consensus {
        public:
            /**
            * Constructs a Proof of Work consensus object. Provides Bitcoin-style
            * Proof of Work to the blockchain module.
            *
            * @param blockTarget the target number of seconds per block
            * @param blockchain a pointer to the blockchain to be used with this
            *        consensus object
            */
            PoW(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain);

            /**
            * In Proof of Work this function checks if the total work of the given
            * block is greater than the total work of the current tip block.
            */
            bool isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip);

            std::string serializeConsensusData(const CryptoKernel::Blockchain::block block);

            /**
            * Checks the following rules:
            *   - block target is correct
            *   - Proof of Work is correct
            *   - Proof of Work is below block target
            *   - Checks the block's total work is correct
            */
            bool checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock);

            Json::Value generateConsensusData(const CryptoKernel::Blockchain::block block, const std::string publicKey);

            /**
            * Pure virtual function that provides a proof of work hash
            * of the given input string
            *
            * @param inputString the string to hash
            * @return the hash of the given input in hex
            */
            virtual std::string powFunction(const std::string inputString) = 0;

            /**
            * Pure virtual function that calculates the proof of work target
            * for a given block.
            *
            * @param previousBlockId the ID of the previous block to the block
            *        to calculate the target for
            * @return the hex target of the block
            */
            virtual std::string calculateTarget(const std::string previousBlockId) = 0;

            /**
            * This class uses Kimoto Gravity Well for difficulty adjustment
            * and SHA256 as its Proof of Work function.
            */
            class KGW_SHA256;

        protected:
            CryptoKernel::Blockchain* blockchain;
            uint64_t blockTarget;
            struct consensusData {
                std::string totalWork;
                std::string target;
                std::string PoW;
                uint64_t nonce;
            };
            consensusData getConsensusData(const CryptoKernel::Blockchain::block block);
            Json::Value consensusDataToJson(const consensusData data);
            std::string calculatePoW(const CryptoKernel::Blockchain::block block);
    };

    class Consensus::PoW::KGW_SHA256 : public PoW {
        public:
            KGW_SHA256(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain);
            std::string powFunction(const std::string inputString);
            std::string calculateTarget(const std::string previousBlockId);
    };
}

#endif // POW_H_INCLUDED
