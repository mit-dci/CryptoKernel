#include <sstream>
#include <math.h>

#include "PoW.h"
#include "../crypto.h"

CryptoKernel::Consensus::PoW::PoW(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain) {
    this->blockTarget = blockTarget;
    this->blockchain = blockchain;
}

bool CryptoKernel::Consensus::PoW::isBlockBetter(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip) {
    const consensusData blockData = getConsensusData(block);
    const consensusData tipData = getConsensusData(tip);
    return CryptoKernel::Math::hex_greater(blockData.totalWork, tipData.totalWork);
}

CryptoKernel::Consensus::PoW::consensusData CryptoKernel::Consensus::PoW::getConsensusData(const CryptoKernel::Blockchain::block block) {
    consensusData data;
    data.PoW = block.consensusData["PoW"].asString();
    data.target = block.consensusData["target"].asString();
    data.totalWork = block.consensusData["totalWork"].asString();
    data.nonce = block.consensusData["nonce"].asUInt64();
    return data;
}

Json::Value CryptoKernel::Consensus::PoW::consensusDataToJson(const CryptoKernel::Consensus::PoW::consensusData data) {
    Json::Value returning;
    returning["PoW"] = data.PoW;
    returning["target"] = data.target;
    returning["totalWork"] = data.totalWork;
    returning["nonce"] = static_cast<unsigned long long int>(data.nonce);
    return returning;
}

std::string CryptoKernel::Consensus::PoW::serializeConsensusData(const CryptoKernel::Blockchain::block block) {
    return block.consensusData["target"].asString();
}

bool CryptoKernel::Consensus::PoW::checkConsensusRules(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock) {
    //Check target
    const consensusData blockData = getConsensusData(block);
    if(blockData.target != calculateTarget(transaction, block.previousBlockId)) {
        return false;
    }

    //Check proof of work
    if(!CryptoKernel::Math::hex_greater(blockData.target, blockData.PoW) || calculatePoW(block) != blockData.PoW) {
        return false;
    }

    //Check total work
    const std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", blockData.target);
    const consensusData tipData = getConsensusData(previousBlock);
    if(blockData.totalWork != CryptoKernel::Math::addHex(inverse, tipData.totalWork)) {
        return false;
    }

    return true;
}

std::string CryptoKernel::Consensus::PoW::calculatePoW(const CryptoKernel::Blockchain::block block) {
    const consensusData blockData = getConsensusData(block);
    std::stringstream buffer;
    buffer << block.id << blockData.nonce;

    return powFunction(buffer.str());
}

Json::Value CryptoKernel::Consensus::PoW::generateConsensusData(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block block, const std::string publicKey) {
    consensusData data;
    data.target = calculateTarget(transaction, block.previousBlockId);

    return consensusDataToJson(data);
}

CryptoKernel::Consensus::PoW::KGW_SHA256::KGW_SHA256(const uint64_t blockTarget, CryptoKernel::Blockchain* blockchain) : CryptoKernel::Consensus::PoW(blockTarget, blockchain) {

}

std::string CryptoKernel::Consensus::PoW::KGW_SHA256::powFunction(const std::string inputString) {
    CryptoKernel::Crypto crypto;
    return crypto.sha256(inputString);
}

std::string CryptoKernel::Consensus::PoW::KGW_SHA256::calculateTarget(Storage::Transaction* transaction, const std::string previousBlockId) {
    const uint64_t minBlocks = 144;
    const uint64_t maxBlocks = 4032;
    const std::string minDifficulty = "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

    CryptoKernel::Blockchain::block currentBlock = blockchain->getBlock(transaction, previousBlockId);
    consensusData currentBlockData = getConsensusData(currentBlock);
    CryptoKernel::Blockchain::block lastSolved = currentBlock;

    if(currentBlock.height < minBlocks)
    {
        return minDifficulty;
    }
    else
    {
        uint64_t blocksScanned = 0;
        std::string difficultyAverage = "0";
        std::string previousDifficultyAverage = "0";
        int64_t actualRate = 0;
        int64_t targetRate = 0;
        double rateAdjustmentRatio = 1.0;
        double eventHorizonDeviation = 0.0;
        double eventHorizonDeviationFast = 0.0;
        double eventHorizonDeviationSlow = 0.0;

        for(unsigned int i = 1; currentBlock.previousBlockId != ""; i++)
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
                difficultyAverage = CryptoKernel::Math::addHex(CryptoKernel::Math::divideHex(CryptoKernel::Math::subtractHex(currentBlockData.target, previousDifficultyAverage), buffer.str()), previousDifficultyAverage);
            }

            previousDifficultyAverage = difficultyAverage;

            actualRate = lastSolved.timestamp - currentBlock.timestamp;
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

            if(currentBlock.previousBlockId == "")
            {
                break;
            }
            currentBlock = blockchain->getBlock(transaction, currentBlock.previousBlockId);
            currentBlockData = getConsensusData(currentBlock);
        }

        std::string newTarget = difficultyAverage;
        if(actualRate != 0 && targetRate != 0)
        {
            std::stringstream buffer;
            buffer << std::hex << actualRate;
            newTarget = CryptoKernel::Math::multiplyHex(newTarget, buffer.str());

            buffer.str("");
            buffer << std::hex << targetRate;
            newTarget = CryptoKernel::Math::divideHex(newTarget, buffer.str());
        }

        if(CryptoKernel::Math::hex_greater(newTarget, minDifficulty))
        {
            newTarget = minDifficulty;
        }

        return newTarget;
    }
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::verifyTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::submitTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction tx) {
    return true;
}

bool CryptoKernel::Consensus::PoW::KGW_SHA256::submitBlock(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block block) {
    return true;
}
