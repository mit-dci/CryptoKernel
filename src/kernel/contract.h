#ifndef CONTRACT_H_INCLUDED
#define CONTRACT_H_INCLUDED

#include <selene.h>

#include "blockchain.h"

namespace CryptoKernel
{
    /**
    * This class runs Lua smart contracts inside transaction inputs to verify validity.
    * Provides methods to evaluate a transactions validity, compile contracts to bytecode
    * and limit execution resources
    */
    class ContractRunner
    {
        public:
            /**
            * Default constructor, creates a ContractRunner instance with access to the given blockchain
            * and with the given resource and execution limits
            *
            * @param blockchain a pointer to the blockchain object to be used for reference
            * @param memoryLimit specify the memory limit in bytes of the virtual machine, defaults to 10MB
            * @param instructionLimit specify the maximum number of instructions a contract can execute, defaults to 35000
            */
            ContractRunner(CryptoKernel::Blockchain* blockchain, const uint64_t memoryLimit = 10485760, const uint64_t instructionLimit = 100000000);

            /**
            * Default destructor
            */
            ~ContractRunner();

            /**
            * Static method for compiling a contract into bytecode such that it can be used in a
            * transaction. Takes the function "verify" in the given script source and returns
            * the LVM bytecode compressed with lz4.
            *
            * @param contractScript a valid CryptoKernel contract script
            * @return the bytecode of the script compressed with lz4
            */
            static std::string compile(const std::string contractScript);

            /**
            * Evaluate all of the input scripts in the given transaction to determine if the transaction
            * is valid according to the contract rules
            *
            * @param dbTx the transaction representing the current blockchain state
            * @param tx the transaction to be verified
            * @return true iff the transaction is valid according to contract scripts, false otherwise
            */
            bool evaluateValid(Storage::Transaction* dbTx, const CryptoKernel::Blockchain::transaction tx);

        private:
            void setupEnvironment(Storage::Transaction* dbTx, const CryptoKernel::Blockchain::transaction tx, const CryptoKernel::Blockchain::output input);
            std::unique_ptr<sel::State> state;
            std::unique_ptr<int> ud;
            static void* allocWrapper(void* thisPointer, void* ptr, size_t osize, size_t nsize);
            void* l_alloc_restricted(void* ud, void* ptr, size_t osize, size_t nsize);
            uint64_t memoryLimit;
            uint64_t pcLimit;
            CryptoKernel::Blockchain* blockchain;
            class BlockchainInterface
            {
                public:
                    BlockchainInterface(CryptoKernel::Blockchain* blockchain) {this->blockchain = blockchain;}
                    std::string getBlock(const std::string id) {return CryptoKernel::Storage::toString(CryptoKernel::Blockchain::blockToJson(blockchain->getBlock(dbTx, id)));}
                    std::string getTransaction(const std::string id) {return CryptoKernel::Storage::toString(CryptoKernel::Blockchain::transactionToJson(blockchain->getTransaction(dbTx, id)));}
                    void setTransaction(Storage::Transaction* dbTx) {this->dbTx = dbTx;};

                private:
                    CryptoKernel::Blockchain* blockchain;
                    Storage::Transaction* dbTx;
            };
            BlockchainInterface* blockchainInterface;
    };
}

#endif // CONTRACT_H_INCLUDED
