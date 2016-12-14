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

#include <ctime>
#include <sstream>
#include <algorithm>
#include <stack>
#include <queue>
#include <fstream>
#include <math.h>
#include <random>

#include "blockchain.h"
#include "crypto.h"
#include "ckmath.h"
#include "contract.h"

CryptoKernel::Blockchain::Blockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime)
{
    status = false;
    blockTarget = blockTime;
    transactions = new CryptoKernel::Storage("./transactiondb");
    blocks = new CryptoKernel::Storage("./blockdb");
    utxos = new CryptoKernel::Storage("./utxodb");
    log = GlobalLog;
}

bool CryptoKernel::Blockchain::loadChain()
{
    chainTipId = blocks->get("tip")["id"].asString();
    if(chainTipId == "")
    {
        if(blocks->get(genesisBlockId)["id"].asString() != genesisBlockId)
        {
            std::ifstream t("genesisblock.txt");
            if(!t.is_open())
            {
                throw std::runtime_error("Could not open genesis block file");
            }
            std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

            if(submitBlock(jsonToBlock(CryptoKernel::Storage::toJson(buffer)), true))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain(): Successfully imported genesis block");
            }
            else
            {
                log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to import genesis block");
            }
        }

        /*block Block = generateMiningBlock("BL8ERakVzl7UZm1JmhdWxsdGIgHdJAUQ9LW6HKTrbtnum6HLAwd7nlTWQfpkhKupsKRjXbL1LnaPVd20ld+4UIM=");
        Block.nonce = 0;

        do
        {
            Block.nonce += 1;
            Block.PoW = calculatePoW(Block);
        }
        while(!CryptoKernel::Math::hex_greater(Block.target, Block.PoW));

        std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", Block.PoW);
        Block.totalWork = inverse;

        submitBlock(Block, true);*/
    }

    reindexChain(chainTipId);

    status = true;

    checkRep();

    return true;
}

void CryptoKernel::Blockchain::checkRep()
{
    assert(blocks != nullptr);
    assert(transactions != nullptr);
    assert(utxos != nullptr);
    assert(log != nullptr);

    assert(getBlock("tip").mainChain);
    assert(getBlock(genesisBlockId).id == genesisBlockId);
    assert(getBlock(genesisBlockId).mainChain);
}

CryptoKernel::Blockchain::~Blockchain()
{
    delete transactions;
    delete blocks;
    delete utxos;
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getUnconfirmedTransactions()
{
    std::vector<CryptoKernel::Blockchain::transaction> returning;
    std::vector<CryptoKernel::Blockchain::transaction>::iterator it;
    for(it = unconfirmedTransactions.begin(); it < unconfirmedTransactions.end(); it++)
    {
        if(!verifyTransaction((*it)))
        {
            it = unconfirmedTransactions.erase(it);
        }
        else
        {
            returning.push_back(*it);
        }
    }

    checkRep();

    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(std::string id)
{
    return jsonToBlock(blocks->get(id));
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(const uint64_t height)
{
    if(height == 1)
    {
        return getBlock(genesisBlockId);
    }
    else
    {
        block currentBlock = getBlock("tip");
        while(currentBlock.height != height && currentBlock.height != 1)
        {
            currentBlock = getBlock(currentBlock.previousBlockId);
        }
        return currentBlock;
    }
}

uint64_t CryptoKernel::Blockchain::getBalance(std::string publicKey)
{
    uint64_t total = 0;

    if(status)
    {
        CryptoKernel::Storage::Iterator *it = utxos->newIterator();
        for(it->SeekToFirst(); it->Valid(); it->Next())
        {
            if(it->value()["publicKey"].asString() == publicKey)
            {
                total += it->value()["value"].asUInt64();
            }
        }
        delete it;

    }

    return total;
}

bool CryptoKernel::Blockchain::verifyTransaction(transaction tx, bool coinbaseTx)
{
    if(calculateTransactionId(tx) != tx.id)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx id is incorrect");
        return false;
    }

    if(transactions->get(tx.id)["id"].asString() == tx.id)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx already exists");
        return false;
    }

    /*time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    if(tx.timestamp < (now - 5 * 60 * 60))
    {
        return false;
    }*/

    uint64_t inputTotal = 0;
    uint64_t outputTotal = 0;
    std::vector<output>::iterator it;

    std::string outputHash = calculateOutputSetId(tx.outputs);

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        if((*it).id != calculateOutputId(*it))
        {
            return false;
        }

        if(utxos->get((*it).id)["id"] == (*it).id || (*it).value < 1)
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Duplicate output in tx");
            //Duplicate output
            return false;
        }
        CryptoKernel::Crypto crypto;
        if(!crypto.setPublicKey((*it).publicKey))
        {
           log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Public key is invalid");
            //Invalid key
            return false;
        }
        outputTotal += (*it).value;
    }

    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        inputTotal += (*it).value;
        CryptoKernel::Crypto crypto;
        crypto.setPublicKey((*it).publicKey);
        if((!crypto.verify((*it).id + outputHash, (*it).signature) && (*it).data["contract"].empty()) || (*it).value <= 0)
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Could not verify input signature");
            return false;
        }
        else
        {
            //Check input id is correct
            if((*it).id != calculateOutputId(*it))
            {
                return false;
            }

            //Check if input has already been spent
            std::string id = utxos->get((*it).id)["id"].asString();
            if(id != (*it).id)
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output has already been spent");
                return false;
            }
        }
    }

    if(!coinbaseTx)
    {
        if(outputTotal > inputTotal)
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): The output total is greater than the input total");
            return false;
        }

        uint64_t fee = inputTotal - outputTotal;
        if(fee < getTransactionFee(tx) * 0.5)
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx fee is too low");
            return false;
        }
    }

    CryptoKernel::ContractRunner lvm(this);
    if(!lvm.evaluateValid(tx))
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Script returned false");
        return false;
    }

    return true;
}

bool CryptoKernel::Blockchain::submitTransaction(transaction tx)
{
    if(verifyTransaction(tx))
    {
        if(transactions->get(tx.id)["id"].asString() == tx.id)
        {
            //Transaction has already been submitted and verified
            log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction that has already been verified");
            return false;
        }
        else
        {
            //Transaction has not already been verified
            bool found = false;

            //Check if transaction is already in the unconfirmed vector
            std::vector<transaction>::iterator it;
            for(it = unconfirmedTransactions.begin(); it < unconfirmedTransactions.end(); it++)
            {
                found = ((*it).id == tx.id);
                if(found)
                {
                    break;
                }
            }

            if(!found)
            {
                unconfirmedTransactions.push_back(tx);
                log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction we didn't already know about");
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction we already know about");
                return true;
            }
        }
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to verify transaction");
        return false;
    }
}

Json::Value CryptoKernel::Blockchain::transactionToJson(transaction tx)
{
    Json::Value returning;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        returning["inputs"].append(outputToJson((*it)));
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        returning["outputs"].append(outputToJson((*it)));
    }

    returning["timestamp"] = static_cast<unsigned long long int>(tx.timestamp);
    returning["id"] = tx.id;

    return returning;
}

Json::Value CryptoKernel::Blockchain::outputToJson(output Output)
{
    Json::Value returning;

    returning["id"] = Output.id;
    returning["value"] = static_cast<unsigned long long int>(Output.value);
    returning["signature"] = Output.signature;
    returning["publicKey"] = Output.publicKey;
    returning["nonce"] = static_cast<unsigned long long int>(Output.nonce);
    returning["data"] = Output.data;

    return returning;
}

std::string CryptoKernel::Blockchain::calculateTransactionId(transaction tx)
{
    std::stringstream buffer;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        buffer << calculateOutputId((*it));
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        buffer << calculateOutputId((*it));
    }

    buffer << tx.timestamp;

    CryptoKernel::Crypto crypto;

    return crypto.sha256(buffer.str());
}

std::string CryptoKernel::Blockchain::calculateOutputId(output Output)
{
    std::stringstream buffer;

    buffer << Output.publicKey << Output.value << Output.nonce << CryptoKernel::Storage::toString(Output.data);

    CryptoKernel::Crypto crypto;
    return crypto.sha256(buffer.str());
}

bool CryptoKernel::Blockchain::submitBlock(block newBlock, bool genesisBlock)
{
    //Check block does not already exist
    if(blocks->get(newBlock.id)["id"].asString() == newBlock.id && blocks->get(newBlock.id)["mainChain"].asBool())
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block already exists");
        return false;
    }

    //Check the previous block exists
    if((blocks->get(newBlock.previousBlockId)["id"].asString() != newBlock.previousBlockId || newBlock.previousBlockId == "") && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Previous block does not exist");
        return false;
    }

    //Check target
    if(newBlock.target != calculateTarget(newBlock.previousBlockId))
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block target is incorrect");
        return false;
    }

    //Check proof of work
    if(!CryptoKernel::Math::hex_greater(newBlock.target, newBlock.PoW) || calculatePoW(newBlock) != newBlock.PoW)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Proof of work is incorrect");
        return false;
    }

    //Check total work
    std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", newBlock.PoW);
    if(newBlock.totalWork != CryptoKernel::Math::addHex(inverse, jsonToBlock(blocks->get(newBlock.previousBlockId)).totalWork) && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Total work is incorrect");
        return false;
    }

    if(newBlock.height != getBlock(newBlock.previousBlockId).height + 1 && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block height is incorrect");
        return false;
    }

    bool onlySave = false;

    if(newBlock.previousBlockId != chainTipId && !genesisBlock)
    {
        //This block does not directly lead on from last block
        //Check if its PoW is bigger than the longest chain
        //If so, reorg, otherwise ignore it
        if(CryptoKernel::Math::hex_greater(newBlock.totalWork, getBlock("tip").totalWork))
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Forking the chain");
            std::string originalTip = getBlock("tip").id;
            reorgChain(newBlock.previousBlockId);
        }
        else
        {
            log->printf(LOG_LEVEL_WARN, "blockchain::submitBlock(): Chain has a lower PoW than current chain");
            onlySave = true;
        }
    }

    //Check the id is correct
    if(calculateBlockId(newBlock) != newBlock.id)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block id is incorrect");
        return false;
    }

    //Check that the timestamp is realistic
    if(newBlock.timestamp < jsonToBlock(blocks->get(newBlock.previousBlockId)).timestamp && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Timestamp is unrealistic");
        return false;
    }

    if(!onlySave)
    {
        uint64_t fees = 0;

        std::vector<transaction>::iterator it;
        for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
        {
            if(!verifyTransaction(*it))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Transaction could not be verified");
                return false;
            }
        }

        //Verify Transactions
        for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
        {
            if(!submitTransaction((*it)))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Transaction could not be submitted");
                return false;
            }
            fees += calculateTransactionFee((*it));
        }

        if(!newBlock.coinbaseTx.inputs.empty())
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase tx has inputs");
            return false;
        }
        else
        {
            uint64_t outputTotal = 0;
            std::vector<output>::iterator it2;
            for(it2 = newBlock.coinbaseTx.outputs.begin(); it2 < newBlock.coinbaseTx.outputs.end(); it2++)
            {
                CryptoKernel::Crypto crypto;
                if(!crypto.setPublicKey((*it2).publicKey))
                {
                    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase public key is invalid");
                    //Invalid key
                    return false;
                }
                outputTotal += (*it2).value;
            }

            if(outputTotal != fees + getBlockReward(newBlock.height))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase output exceeds limit");
                return false;
            }

            if(verifyTransaction(newBlock.coinbaseTx, true))
            {
                confirmTransaction(newBlock.coinbaseTx, true);
            }
            else
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Could not verify coinbase transaction");
                return false;
            }
        }

        //Move transactions from unconfirmed to confirmed and add transaction utxos to db
        for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
        {
            if(!confirmTransaction((*it)))
            {
                reindexChain(chainTipId);
                return false;
            }
        }
    }

    newBlock.mainChain = false;

    if(!onlySave)
    {
        newBlock.mainChain = true;
        chainTipId = newBlock.id;
        blocks->store("tip", blockToJson(newBlock));
    }

    blocks->store(newBlock.id, blockToJson(newBlock));

    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): successfully submitted block: " + CryptoKernel::Storage::toString(blockToJson(newBlock)));

    checkRep();

    return true;
}

std::string CryptoKernel::Blockchain::calculateBlockId(block Block)
{
    std::stringstream buffer;

    std::vector<transaction>::iterator it;
    for(it = Block.transactions.begin(); it < Block.transactions.end(); it++)
    {
        //This should probably be converted to calculate a merkle root
        buffer << calculateTransactionId((*it));
    }

    buffer << calculateTransactionId(Block.coinbaseTx);

    buffer << Block.previousBlockId << Block.timestamp << Block.target << Block.height;

    CryptoKernel::Crypto crypto;
    return crypto.sha256(buffer.str());
}

Json::Value CryptoKernel::Blockchain::blockToJson(block Block, bool PoW)
{
    Json::Value returning;

    returning["id"] = Block.id;
    returning["height"] = Block.height;
    returning["previousBlockId"] = Block.previousBlockId;
    returning["timestamp"] = static_cast<unsigned long long int>(Block.timestamp);
    std::vector<transaction>::iterator it;
    for(it = Block.transactions.begin(); it < Block.transactions.end(); it++)
    {
        returning["transactions"].append(transactionToJson((*it)));
    }

    if(!PoW)
    {
        returning["PoW"] = Block.PoW;
        returning["totalWork"] = Block.totalWork;
    }

    returning["target"] = Block.target;
    returning["coinbaseTx"] = transactionToJson(Block.coinbaseTx);
    returning["nonce"] = static_cast<unsigned long long int>(Block.nonce);

    if(!PoW)
    {
        returning["mainChain"] = Block.mainChain;
    }

    return returning;
}

bool CryptoKernel::Blockchain::confirmTransaction(transaction tx, bool coinbaseTx)
{
    //Check if transaction is already confirmed
    if(transactions->get(tx.id)["id"] == tx.id)
    {
        return false;
    }

    //Prevent against double spends by checking again before confirmation
    if(!verifyTransaction(tx, coinbaseTx))
    {
        return false;
    }

    //"Spend" UTXOs
    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        utxos->erase((*it).id);
    }

    //Add new outputs to UTXOs
    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        utxos->store((*it).id, outputToJson((*it)));
    }

    //Commit transaction
    transactions->store(tx.id, transactionToJson(tx));

    //Remove transaction from unconfirmed transactions vector
    std::vector<transaction>::iterator it2;
    for(it2 = unconfirmedTransactions.begin(); it2 < unconfirmedTransactions.end();)
    {
        if((*it2).id == tx.id)
        {
            it2 = unconfirmedTransactions.erase(it2);
        }
        else
        {
            ++it2;
        }
    }

    return true;
}

bool CryptoKernel::Blockchain::reindexChain(std::string newTipId)
{
    std::stack<block> blockList;

    block currentBlock = jsonToBlock(blocks->get(newTipId));
    while(currentBlock.previousBlockId != "")
    {
        blockList.push(currentBlock);
        currentBlock = jsonToBlock(blocks->get(currentBlock.previousBlockId));
    }

    if(currentBlock.id != genesisBlockId)
    {
        log->printf(LOG_LEVEL_WARN, "blockchain::reindexChain(): Chain has incorrect genesis block");
        return false;
    }

    status = false;

    chainTipId = "";

    delete transactions;
    CryptoKernel::Storage::destroy("./transactiondb");
    transactions = new CryptoKernel::Storage("./transactiondb");

    delete utxos;
    CryptoKernel::Storage::destroy("./utxodb");
    utxos = new CryptoKernel::Storage("./utxodb");

    delete blocks;
    CryptoKernel::Storage::destroy("./blockdb");
    blocks = new CryptoKernel::Storage("./blockdb");

    submitBlock(currentBlock, true);

    while(!blockList.empty())
    {
        currentBlock = blockList.top();
        blockList.pop();
        if(!submitBlock(currentBlock))
        {
            return false;
        }
    }

    status = true;

    return true;
}

bool CryptoKernel::Blockchain::reorgChain(std::string newTipId)
{
    std::stack<block> blockList;

    //Find common fork block
    block currentBlock = getBlock(newTipId);
    while(!currentBlock.mainChain && currentBlock.previousBlockId != "")
    {
        blockList.push(currentBlock);
        std::string id = currentBlock.previousBlockId;
        currentBlock = getBlock(id);
        if(currentBlock.id != id)
        {
            return false;
        }
    }

    if(blocks->get(currentBlock.id)["id"].asString() != currentBlock.id)
    {
        return false;
    }

    //Reverse blocks to that point
    while(getBlock("tip").id != currentBlock.id)
    {
        if(!reverseBlock())
        {
            return false;
        }
    }

    //Submit new blocks
    while(blockList.size() > 0)
    {
        if(!submitBlock(blockList.top()))
        {
            return false;
        }
        blockList.pop();
    }

    return true;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::jsonToBlock(Json::Value Block)
{
    block returning;

    returning.id = Block["id"].asString();
    returning.previousBlockId = Block["previousBlockId"].asString();
    returning.timestamp = Block["timestamp"].asUInt64();
    returning.height = Block["height"].asUInt();
    returning.PoW = Block["PoW"].asString();
    returning.target = Block["target"].asString();
    returning.totalWork = Block["totalWork"].asString();

    for(unsigned int i = 0; i < Block["transactions"].size(); i++)
    {
        returning.transactions.push_back(jsonToTransaction(Block["transactions"][i]));
    }

    returning.coinbaseTx = jsonToTransaction(Block["coinbaseTx"]);
    returning.nonce = Block["nonce"].asUInt64();
    returning.mainChain = Block["mainChain"].asBool();

    return returning;
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::jsonToTransaction(Json::Value tx)
{
    transaction returning;

    returning.id = tx["id"].asString();
    returning.timestamp = tx["timestamp"].asUInt64();

    for(unsigned int i = 0; i < tx["inputs"].size(); i++)
    {
        returning.inputs.push_back(jsonToOutput(tx["inputs"][i]));
    }

    for(unsigned int i = 0; i < tx["outputs"].size(); i++)
    {
        returning.outputs.push_back(jsonToOutput(tx["outputs"][i]));
    }

    return returning;
}

CryptoKernel::Blockchain::output CryptoKernel::Blockchain::jsonToOutput(Json::Value Output)
{
    output returning;

    returning.id = Output["id"].asString();
    returning.publicKey = Output["publicKey"].asString();
    returning.signature = Output["signature"].asString();
    returning.data = Output["data"];
    returning.value = Output["value"].asUInt64();
    returning.nonce = Output["nonce"].asUInt64();

    return returning;
}

std::string CryptoKernel::Blockchain::calculateTarget(std::string previousBlockId)
{
    const uint64_t minBlocks = 144;
    const uint64_t maxBlocks = 4032;
    const std::string minDifficulty = "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

    block currentBlock = getBlock(previousBlockId);
    block lastSolved = currentBlock;

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
                difficultyAverage = currentBlock.target;
            }
            else
            {
                std::stringstream buffer;
                buffer << std::hex << i;
                difficultyAverage = CryptoKernel::Math::addHex(CryptoKernel::Math::divideHex(CryptoKernel::Math::subtractHex(currentBlock.target, previousDifficultyAverage), buffer.str()), previousDifficultyAverage);
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
            currentBlock = getBlock(currentBlock.previousBlockId);
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

uint64_t CryptoKernel::Blockchain::getTransactionFee(transaction tx)
{
    uint64_t fee = 0;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        if((*it).value < 100000)
        {
            fee += 10000;
        }

        fee += CryptoKernel::Storage::toString((*it).data).size() * 1000;
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        if((*it).value < 100000)
        {
            fee += 10000;
        }

        fee += CryptoKernel::Storage::toString((*it).data).size() * 1000;
    }

    return fee;
}

uint64_t CryptoKernel::Blockchain::calculateTransactionFee(transaction tx)
{
    uint64_t inputTotal = 0;
    uint64_t outputTotal = 0;
    std::vector<output>::iterator it;

    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        inputTotal += (*it).value;
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        outputTotal += (*it).value;
    }

    return inputTotal - outputTotal;
}

std::string CryptoKernel::Blockchain::calculatePoW(block Block)
{
    std::stringstream buffer;
    buffer << Block.id << Block.nonce;

    return PoWFunction(buffer.str());
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::generateMiningBlock(std::string publicKey)
{
    block returning;

    returning.transactions = unconfirmedTransactions;
    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    returning.timestamp = now;

    block previousBlock;
    previousBlock = jsonToBlock(blocks->get("tip"));

    returning.height = previousBlock.height + 1;
    returning.previousBlockId = previousBlock.id;
    returning.target = calculateTarget(previousBlock.id);

    output toMe;
    toMe.value = getBlockReward(returning.height);

    std::vector<transaction>::iterator it;
    for(it = returning.transactions.begin(); it < returning.transactions.end(); it++)
    {
        toMe.value += calculateTransactionFee((*it));
    }
    toMe.publicKey = publicKey;

    std::default_random_engine generator(now);
    std::uniform_int_distribution<unsigned int> distribution(0, UINT_MAX);
    toMe.nonce = distribution(generator);

    toMe.id = calculateOutputId(toMe);

    transaction coinbaseTx;
    coinbaseTx.outputs.push_back(toMe);
    coinbaseTx.timestamp = now;
    coinbaseTx.id = calculateTransactionId(coinbaseTx);
    returning.coinbaseTx = coinbaseTx;

    returning.id = calculateBlockId(returning);

    return returning;
}

std::vector<CryptoKernel::Blockchain::output> CryptoKernel::Blockchain::getUnspentOutputs(std::string publicKey)
{
    std::vector<output> returning;

    if(status)
    {
        CryptoKernel::Storage::Iterator *it = utxos->newIterator();
        for(it->SeekToFirst(); it->Valid(); it->Next())
        {
            if(it->value()["publicKey"] == publicKey)
            {
                returning.push_back(jsonToOutput(it->value()));
            }
        }
        delete it;
    }

    return returning;
}
std::string CryptoKernel::Blockchain::calculateOutputSetId(std::vector<output> outputs)
{
    std::queue<output> orderedOutputs;

    while(!outputs.empty())
    {
        std::string lowestId = outputs[0].id;
        std::vector<output>::iterator it;
        for(it = outputs.begin(); it < outputs.end(); it++)
        {
            if(!CryptoKernel::Math::hex_greater((*it).id, lowestId))
            {
                lowestId = (*it).id;
            }
        }

        for(it = outputs.begin(); it < outputs.end(); it++)
        {
            if((*it).id == lowestId)
            {
                orderedOutputs.push(*it);
                it = outputs.erase(it);
            }
        }
    }

    std::string returning = "";
    CryptoKernel::Crypto crypto;
    while(!orderedOutputs.empty())
    {
        returning = crypto.sha256(returning + orderedOutputs.front().id);
        orderedOutputs.pop();
    }

    return returning;
}

bool CryptoKernel::Blockchain::reverseBlock()
{
    block tip = getBlock("tip");

    std::vector<output>::iterator it2;
    for(it2 = tip.coinbaseTx.outputs.begin(); it2 < tip.coinbaseTx.outputs.end(); it2++)
    {
        if(!utxos->erase((*it2).id))
        {
            reindexChain(tip.id);
            return false;
        }
    }

    std::vector<transaction>::iterator it;
    for(it = tip.transactions.begin(); it < tip.transactions.end(); it++)
    {
        for(it2 = (*it).outputs.begin(); it2 < (*it).outputs.end(); it2++)
        {
            if(!utxos->erase((*it2).id))
            {
                reindexChain(tip.id);
                return false;
            }
        }

        for(it2 = (*it).inputs.begin(); it2 < (*it).inputs.end(); it2++)
        {
            output toSave = (*it2);
            toSave.signature = "";
            utxos->store(toSave.id, outputToJson(toSave));
        }

        if(!transactions->erase((*it).id))
        {
            reindexChain(tip.id);
            return false;
        }

        if(!submitTransaction(*it))
        {
            reindexChain(tip.id);
            return false;
        }
    }

    blocks->store("tip", blockToJson(getBlock(tip.previousBlockId)));
    chainTipId = tip.previousBlockId;

    tip.mainChain = false;
    blocks->store(tip.id, blockToJson(tip));

    return true;
}

CryptoKernel::Storage::Iterator* CryptoKernel::Blockchain::newIterator()
{
    return blocks->newIterator();
}
