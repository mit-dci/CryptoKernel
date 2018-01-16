#include "../crypto.h"
#include "AVRR.h"

CryptoKernel::Consensus::AVRR::AVRR(const std::set<std::string>& verifiers,
                                    const uint64_t blockTarget) {
    this->verifiers = verifiers;
    this->blockTarget = blockTarget;
}

bool CryptoKernel::Consensus::AVRR::isBlockBetter(const CryptoKernel::Blockchain::dbBlock&
        block, const CryptoKernel::Blockchain::dbBlock& tip) {
    return (block.getConsensusData()["sequenceNumber"].asUInt64() <
            tip.getConsensusData()["sequenceNumber"].asUInt64() &&
            block.getHeight() >= tip.getHeight());
}

std::string CryptoKernel::Consensus::AVRR::serializeConsensusData(
    const CryptoKernel::Blockchain::block& block) {
    return block.getConsensusData()["publicKey"].asString() +
           block.getConsensusData()["sequenceNumber"].asString();
}

bool CryptoKernel::Consensus::AVRR::checkConsensusRules(const
        CryptoKernel::Blockchain::block& block,
        const CryptoKernel::Blockchain::block& previousBlock) {
    const consensusData blockData = getConsensusData(block);
    const consensusData previousBlockData = getConsensusData(previousBlock);
    if(blockData.sequenceNumber <= previousBlockData.sequenceNumber ||
            (block.getTimestamp() / blockTarget) != blockData.sequenceNumber) {
        return false;
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);
    if(now < block.getTimestamp()) {
        return false;
    }

    if(getVerifier(block) != blockData.publicKey) {
        return false;
    }

    CryptoKernel::Crypto crypto;
    crypto.setPublicKey(blockData.publicKey);
    if(!crypto.verify(block.getId().toString(), blockData.signature)) {
        return false;
    }

    return true;
}

Json::Value CryptoKernel::Consensus::AVRR::generateConsensusData(
    const CryptoKernel::Blockchain::block& block, const std::string& publicKey) {
    consensusData data;
    data.publicKey = publicKey;
    data.sequenceNumber = block.getTimestamp() / blockTarget;

    return consensusDataToJson(data);
}

std::string CryptoKernel::Consensus::AVRR::getVerifier(const
        CryptoKernel::Blockchain::block& block) {
    const consensusData blockData = getConsensusData(block);
    const uint64_t verifierId = blockData.sequenceNumber % verifiers.size();

    return *std::next(verifiers.begin(), verifierId);
}

CryptoKernel::Consensus::AVRR::consensusData
CryptoKernel::Consensus::AVRR::getConsensusData(const CryptoKernel::Blockchain::block&
        block) {
    consensusData returning;
    const Json::Value data = block.getConsensusData();
    returning.publicKey = data["publicKey"].asString();
    returning.signature = data["signature"].asString();
    returning.sequenceNumber = data["sequenceNumber"].asUInt64();
    return returning;
}

Json::Value CryptoKernel::Consensus::AVRR::consensusDataToJson(
    const consensusData& data) {
    Json::Value returning;
    returning["publicKey"] = data.publicKey;
    returning["signature"] = data.signature;
    returning["sequenceNumber"] = data.sequenceNumber;
    return returning;
}

bool CryptoKernel::Consensus::AVRR::verifyTransaction(const
        CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::AVRR::confirmTransaction(const
        CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::AVRR::submitTransaction(const
        CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::AVRR::submitBlock(const CryptoKernel::Blockchain::block&
        block) {
    return true;
}
