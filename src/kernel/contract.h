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
            * @param memoryLimit specify the memory limit in bytes of the virtual machine, defaults to 1MB
            * @param instructionLimit specify the maximum number of instructions a contract can execute, defaults to 35000
            */
            ContractRunner(const CryptoKernel::Blockchain* blockchain, const uint64_t memoryLimit = 1048576, const uint64_t instructionLimit = 35000);

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
            * @param tx the transaction to be verified
            * @return true iff the transaction is valid according to contract scripts, false otherwise
            */
            bool evaluateValid(const CryptoKernel::Blockchain::transaction tx);

        private:
            static void setupEnvironment(sel::State* stateEnv);
            std::unique_ptr<sel::State> state;
            std::unique_ptr<int> ud;
            static void* allocWrapper(void* thisPointer, void* ptr, size_t osize, size_t nsize);
            void* l_alloc_restricted(void* ud, void* ptr, size_t osize, size_t nsize);
            uint64_t memoryLimit;
    };
}

#endif // CONTRACT_H_INCLUDED
