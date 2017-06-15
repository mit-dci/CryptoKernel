#include <sstream>
#include <math.h>

#include "PoW.h"
#include "../crypto.h"

CryptoKernel::Consensus::PoW::PoW(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain) {
    this->blockTarget = blockTarget;
    this->blockchain = blockchain;
}

bool CryptoKernel::Consensus::PoW::isBlockBetter(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& tip) {
    const consensusData blockData = getConsensusData(block);
    const consensusData tipData = getConsensusData(tip);
    return blockData.totalWork > tipData.totalWork;
}

CryptoKernel::Consensus::PoW::consensusData CryptoKernel::Consensus::PoW::getConsensusData(const CryptoKernel::Blockchain::block& block) {
    consensusData data;
    const Json::Value consensusJson = block.getConsensusData();
    try {
        data.target = CryptoKernel::BigNum(consensusJson["target"].asString());
        data.totalWork = CryptoKernel::BigNum(consensusJson["totalWork"].asString());
        data.nonce = consensusJson["nonce"].asUInt64();
    } catch(const Json::Exception& e) {
        throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
    }
    return data;
}

CryptoKernel::Consensus::PoW::consensusData CryptoKernel::Consensus::PoW::getConsensusData(const CryptoKernel::Blockchain::dbBlock& block) {
    consensusData data;
    const Json::Value consensusJson = block.getConsensusData();
    try {
        data.target = CryptoKernel::BigNum(consensusJson["target"].asString());
        data.totalWork = CryptoKernel::BigNum(consensusJson["totalWork"].asString());
        data.nonce = consensusJson["nonce"].asUInt64();
    } catch(const Json::Exception& e) {
        throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
    }
    return data;
}

Json::Value CryptoKernel::Consensus::PoW::consensusDataToJson(const CryptoKernel::Consensus::PoW::consensusData& data) {
    Json::Value returning;
    returning["target"] = data.target.toString();
    returning["totalWork"] = data.totalWork.toString();
    returning["nonce"] = static_cast<unsigned long long int>(data.nonce);
    return returning;
}

bool CryptoKernel::Consensus::PoW::checkConsensusRules(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& previousBlock) {
    //Check target
    try {
        const consensusData blockData = getConsensusData(block);
        if(blockData.target != calculateTarget(transaction, block.getPreviousBlockId())) {
            return false;
        }

        //Check proof of work
        if(blockData.target <= calculatePoW(block, blockData.nonce)) {
            return false;
        }

        //Check total work
        const consensusData tipData = getConsensusData(previousBlock);
        const BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - blockData.target;
        if(blockData.totalWork != inverse + tipData.totalWork) {
            return false;
        }

        return true;
    } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
        return false;
    }
}

CryptoKernel::BigNum CryptoKernel::Consensus::PoW::calculatePoW(const CryptoKernel::Blockchain::block& block, const uint64_t nonce) {
    std::stringstream buffer;
    buffer << block.getId().toString() << nonce;
    return powFunction(buffer.str());
}

Json::Value CryptoKernel::Consensus::PoW::generateConsensusData(Storage::Transaction* transaction, const CryptoKernel::BigNum& previousBlockId, const std::string& publicKey) {
    consensusData data;
    data.target = calculateTarget(transaction, previousBlockId);

    return consensusDataToJson(data);
}

CryptoKernel::Consensus::PoW::KGW_SHA256::KGW_SHA256(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain) : CryptoKernel::Consensus::PoW(blockTarget, blockchain) {

}

CryptoKernel::BigNum CryptoKernel::Consensus::PoW::KGW_SHA256::powFunction(const std::string& inputString) {
    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(inputString));
}

CryptoKernel::BigNum CryptoKernel::Consensus::PoW::KGW_SHA256::calculateTarget(Storage::Transaction* transaction, const CryptoKernel::BigNum& previousBlockId) {
    const uint64_t minBlocks = 144;
    const uint64_t maxBlocks = 4032;
    const CryptoKernel::BigNum minDifficulty = CryptoKernel::BigNum("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    CryptoKernel::Blockchain::dbBlock currentBlock = blockchain->getBlockDB(transaction, previousBlockId.toString());
    consensusData currentBlockData = getConsensusData(currentBlock);
    CryptoKernel::Blockchain::dbBlock lastSolved = currentBlock;

    if(currentBlock.getHeight() < minBlocks && currentBlock.getHeight() != 0)
    {
        return minDifficulty;
    }
    else
    {
        uint64_t blocksScanned = 0;
        CryptoKernel::BigNum difficultyAverage = CryptoKernel::BigNum("0");
        CryptoKernel::BigNum previousDifficultyAverage = CryptoKernel::BigNum("0");
        int64_t actualRate = 0;
        int64_t targetRate = 0;
        double rateAdjustmentRatio = 1.0;
        double eventHorizonDeviation = 0.0;
        double eventHorizonDeviationFast = 0.0;
        double eventHorizonDeviationSlow = 0.0;

        for(unsigned int i = 1; currentBlock.getHeight() != 1; i++)
        {
            if(i > maxBlocks)
            {
                break;
            }

            blocksScanned++;

            if(i == 1)
            {
                difficultyAverage = currentBlockData.target;
            }
            else
            {
                std::stringstream buffer;
                buffer << std::hex << i;
                difficultyAverage = ((currentBlockData.target - previousDifficultyAverage) / CryptoKernel::BigNum(buffer.str())) + previousDifficultyAverage;
            }

            previousDifficultyAverage = difficultyAverage;

            actualRate = lastSolved.getTimestamp() - currentBlock.getTimestamp();
            targetRate = blockTarget * blocksScanned;
            rateAdjustmentRatio = 1.0;

            if(actualRate < 0)
            {
                actualRate = 0;
            }

            if(actualRate != 0 && targetRate != 0)
            {
                rateAdjustmentRatio = double(targetRate) / double(actualRate);
            }

            eventHorizonDeviation = 1 + (0.7084 * pow((double(blocksScanned)/double(minBlocks)), -1.228));
            eventHorizonDeviationFast = eventHorizonDeviation;
            eventHorizonDeviationSlow = 1 / eventHorizonDeviation;

            if(blocksScanned >= minBlocks)
            {
                if((rateAdjustmentRatio <= eventHorizonDeviationSlow) || (rateAdjustmentRatio >= eventHorizonDeviationFast))
                {
                    break;
                }
            }

            if(currentBlock.getHeight() == 1)
            {
                break;
            }
            currentBlock = blockchain->getBlockDB(transaction, currentBlock.getPreviousBlockId().toString());
            currentBlockData = getConsensusData(currentBlock);
        }

        CryptoKernel::BigNum newTarget = difficultyAverage;
        if(actualRate != 0 && targetRate != 0)
        {
            std::stringstream buffer;
            buffer << std::hex << actualRate;
            newTarget = newTarget * CryptoKernel::BigNum(buffer.str());

            buffer.str("");
            buffer << std::hex << targetRate;
            newTarget = newTarget / CryptoKernel::BigNum(buffer.str());
        }

        if(newTarget > minDifficulty)
        {
            newTarget = minDifficulty;
        }

        return newTarget;
    }
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::verifyTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::submitTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::submitBlock(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block) {
    return true;
}
