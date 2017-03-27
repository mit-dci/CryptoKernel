#include <sstream>

#include "PoW.h"

CryptoKernel::PoW::PoW(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain) {
    this->blockTarget = blockTarget;
    this->blockchain = blockchain;
}

bool CryptoKernel::PoW::isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip) {
    const consensusData blockData = getConsensusData(block);
    const consensusData tipData = getConsensusData(tip);
    return CryptoKernel::Math::hex_greater(blockData.totalWork, tipData.totalWork);
}

CryptoKernel::PoW::consensusData CryptoKernel::PoW::getConsensusData(const CryptoKernel::Blockchain::block block) {
    consensusData data;
    data.PoW = block.consensusData["PoW"].asString();
    data.target = block.consensusData["target"].asString();
    data.totalWork = block.consensusData["totalWork"].asString();
    data.nonce = block.consensusData["nonce"].asUInt64();
    return data;
}

Json::Value CryptoKernel::PoW::consensusDataToJson(const CryptoKernel::PoW::consensusData data) {
    Json::Value returning;
    returning["PoW"] = data.PoW;
    returning["target"] = data.target;
    returning["totalWork"] = data.totalWork;
    returning["nonce"] = static_cast<unsigned long long int>(data.nonce);
    return returning;
}

std::string CryptoKernel::PoW::serializeConsensusData(const CryptoKernel::Blockchain::block block) {
    return block.consensusData["target"].asString();
}

bool CryptoKernel::PoW::checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock) {
    //Check target
    const consensusData blockData = getConsensusData(block);
    if(blockData.target != calculateTarget(block.previousBlockId)) {
        return false;
    }

    //Check proof of work
    if(!CryptoKernel::Math::hex_greater(blockData.target, blockData.PoW) || calculatePoW(block) != blockData.PoW) {
        return false;
    }

    //Check total work
    const std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", blockData.PoW);
    const consensusData tipData = getConsensusData(previousBlock);
    if(blockData.totalWork != CryptoKernel::Math::addHex(inverse, tipData.PoW)) {
        return false;
    }

    return true;
}

std::string CryptoKernel::PoW::calculatePoW(const CryptoKernel::Blockchain::block block) {
    const consensusData blockData = getConsensusData(block);
    std::stringstream buffer;
    buffer << block.id << blockData.nonce;

    return powFunction(buffer.str());
}

Json::Value CryptoKernel::PoW::generateConsensusData(const CryptoKernel::Blockchain::block block, const std::string publicKey) {
    consensusData data;
    data.target = calculateTarget(block.previousBlockId);

    return consensusDataToJson(data);
}
