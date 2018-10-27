#ifndef POW_H_INCLUDED
#define POW_H_INCLUDED

#include <thread>

#include "../blockchain.h"

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
    * @param miner a flag to determine whether the consensus object should mine
    * @param pubKey if the miner is enabled, rewards will be sent to this pubKey
    */
    PoW(const uint64_t blockTarget,
        CryptoKernel::Blockchain* blockchain,
        const bool miner,
        const std::string& pubKey,
        CryptoKernel::Log* log);

    virtual ~PoW();

    /**
    * In Proof of Work this function checks if the total work of the given
    * block is greater than the total work of the current tip block.
    */
    bool isBlockBetter(Storage::Transaction* transaction,
                       const CryptoKernel::Blockchain::block& block,
                       const CryptoKernel::Blockchain::dbBlock& tip);

    std::string serializeConsensusData(const CryptoKernel::Blockchain::block& block);

    /**
    * Checks the following rules:
    *   - block target is correct
    *   - Proof of Work is correct
    *   - Proof of Work is below block target
    *   - Checks the block's total work is correct
    */
    bool checkConsensusRules(Storage::Transaction* transaction,
                             CryptoKernel::Blockchain::block& block,
                             const CryptoKernel::Blockchain::dbBlock& previousBlock);

    Json::Value generateConsensusData(Storage::Transaction* transaction,
                                      const CryptoKernel::BigNum& previousBlockId, const std::string& publicKey);

    /**
    * Pure virtual function that provides a proof of work hash
    * of the given input string
    *
    * @param inputString the string to hash
    * @return the hash of the given input in hex
    */
    virtual CryptoKernel::BigNum powFunction(const std::string& inputString) = 0;

    /**
    * Pure virtual function that calculates the proof of work target
    * for a given block.
    *
    * @param previousBlockId the ID of the previous block to the block
    *        to calculate the target for
    * @return the hex target of the block
    */
    virtual CryptoKernel::BigNum calculateTarget(Storage::Transaction* transaction,
            const BigNum& previousBlockId) = 0;

    /**
    * This class uses Kimoto Gravity Well for difficulty adjustment
    * and SHA256 as its Proof of Work function.
    */
    class KGW_SHA256;

    /**
     * This class uses Kimoto Gravity Well for difficulty adjustment
    *  and Lyra2REv2 as its Proof of Work function.
     */
    class KGW_LYRA2REV2;

    /**
    * Calculate the PoW for a given block
    *
    * @param block the block to calculate the Proof of Work of
    * @return a hex string representing the PoW hash of the given block
    */
    CryptoKernel::BigNum calculatePoW(const CryptoKernel::Blockchain::block& block,
                                      const uint64_t nonce);

    virtual void start();
protected:
    CryptoKernel::Blockchain* blockchain;
    CryptoKernel::Log* log;
    uint64_t blockTarget;
    struct consensusData {
        BigNum totalWork;
        BigNum target;
        uint64_t nonce;
    };
    consensusData getConsensusData(const CryptoKernel::Blockchain::block& block);
    consensusData getConsensusData(const CryptoKernel::Blockchain::dbBlock& block);
    Json::Value consensusDataToJson(const consensusData& data);

private:
    bool running;
    void miner();
    std::string pubKey;
    std::unique_ptr<std::thread> minerThread;
};

class Consensus::PoW::KGW_SHA256 : public PoW {
public:
    KGW_SHA256(const uint64_t blockTarget,
               CryptoKernel::Blockchain* blockchain,
               const bool miner,
               const std::string& pubKey,
               CryptoKernel::Log* log);

    /**
    * Uses SHA256 to calculate the hash
    */
    virtual CryptoKernel::BigNum powFunction(const std::string& inputString);

    /**
    * Uses Kimoto Gravity Well to retarget the difficulty
    */
    virtual CryptoKernel::BigNum calculateTarget(Storage::Transaction* transaction,
                                         const BigNum& previousBlockId);

    /**
    * Has no effect, always returns true
    */
    bool verifyTransaction(Storage::Transaction* transaction,
                           const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool confirmTransaction(Storage::Transaction* transaction,
                            const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool submitTransaction(Storage::Transaction* transaction,
                           const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool submitBlock(Storage::Transaction* transaction,
                     const CryptoKernel::Blockchain::block& block);
};

class Consensus::PoW::KGW_LYRA2REV2 : public Consensus::PoW::KGW_SHA256 {
    public:
        KGW_LYRA2REV2(const uint64_t blockTarget,
                      CryptoKernel::Blockchain* blockchain,
                      const bool miner,
                      const std::string& pubKey,
                      CryptoKernel::Log* log);

        /**
        * Uses Lyra2REv2 to calculate the hash
        */
        virtual CryptoKernel::BigNum powFunction(const std::string& inputString);
};

}

#endif // POW_H_INCLUDED
