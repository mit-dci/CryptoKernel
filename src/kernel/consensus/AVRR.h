#ifndef AVRR_H_INCLUDED
#define AVRR_H_INCLUDED

#include "../blockchain.h"

namespace CryptoKernel {
/**
* Implements Authorized Verifier Round-Robin (AVRR) consensus algorithm. Under
* this scheme, every verifier must sign their blocks and their public key must
* be authorized. The verifier responsible for creating the next block is decided
* in a round-robin. If a verifier goes offline and misses their slot, they can be
* skipped by the next verifier in the round.
*/
class Consensus::AVRR : public Consensus {
public:
    /**
    * Constructs a AVRR consensus object with the given set
    * of authorised verifier public keys
    *
    * @param verifiers the set of verifier public keys
    * @param blockTarget the number of seconds between each block
    */
    AVRR(const std::set<std::string>& verifiers, const uint64_t blockTarget);

    /**
    * In AVRR, a block displaces the current block tip if it is at least as high
    * as the current tip but has a lower sequence number. This ensures that an earlier
    * verifier's block in the round-robin always takes precedence but that verifiers
    * who miss their block submission slot cannot retroactively submit a block.
    */
    bool isBlockBetter(const CryptoKernel::Blockchain::dbBlock& block,
                       const CryptoKernel::Blockchain::dbBlock& tip);

    std::string serializeConsensusData(const CryptoKernel::Blockchain::block& block);

    /**
    * Checks the following rules:
    *   - The block's sequence number is greater than the previous block
    *   - Checks that the block is from an authorized verifier
    *   - Checks the block's signature
    *   - Checks the block's timestamp with the system clock to make sure it is not
    *     from the future
    */
    bool checkConsensusRules(const CryptoKernel::Blockchain::block& block,
                             const CryptoKernel::Blockchain::block& previousBlockId);

    Json::Value generateConsensusData(const CryptoKernel::Blockchain::block& block,
                                      const std::string& publicKey);

    /**
    * Returns the public key of the verifier for the given block
    *
    * @param block the block to get the verifier of
    * @return the public key of the verifier
    */
    std::string getVerifier(const CryptoKernel::Blockchain::block& block);

    /**
    * Has no effect, always returns true
    */
    bool verifyTransaction(const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool confirmTransaction(const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool submitTransaction(const CryptoKernel::Blockchain::transaction& tx);

    /**
    * Has no effect, always returns true
    */
    bool submitBlock(const CryptoKernel::Blockchain::block& block);
private:
    std::set<std::string> verifiers;
    uint64_t blockTarget;
    struct consensusData {
        uint64_t sequenceNumber;
        std::string signature;
        std::string publicKey;
    };
    consensusData getConsensusData(const CryptoKernel::Blockchain::block& block);
    Json::Value consensusDataToJson(const consensusData& data);
};
}

#endif // AVRR_H_INCLUDED
