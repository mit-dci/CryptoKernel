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
    chainLock.lock();
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

            t.close();
        }

        /*CryptoKernel::Crypto crypto;
        crypto.setPublicKey("BLNz4IiBnUDanMovX5LQ9KCev1bUVO/70r0WqXv2Gc96SnsPuayoXXYlIivrQ9C8vhOm7scoXm3QXMgid2vsvEs=");
        crypto.setPrivateKey("");
        cbKey = crypto.getPublicKey();
        block Block = generateMiningBlock(crypto.getPublicKey());
        Block.signature = crypto.sign(Block.id);

        submitBlock(Block, true);*/
    }

    // jlovejoy@mit.edu
    verifiers.insert("BGlIWaa5zawgmGqOofUcHeuKrarBuwTkjyqUJR3MJWpey/gdYIr2uJeQD6o4PiUeSyhVRuRs6qN74rwO9gvItks=");

    reindexChain(chainTipId);

    status = true;

    checkRep();

    chainLock.unlock();

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
    chainLock.lock();
    delete transactions;
    delete blocks;
    delete utxos;
    chainLock.unlock();
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getUnconfirmedTransactions()
{
    chainLock.lock();
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

    chainLock.unlock();

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
        chainLock.lock();
        const std::string id = blocks->get("height_" + std::to_string(height)).asString();
        const block returning = getBlock(id);
        chainLock.unlock();
        return returning;
    }
}

uint64_t CryptoKernel::Blockchain::getBalance(std::string publicKey)
{
    chainLock.lock();
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

    chainLock.unlock();

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

    if(tx.inputs.size() < 1 && !coinbaseTx)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx has no inputs");
        return false;
    }

    if(tx.outputs.size() < 1)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx has no outputs");
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
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output has incorrect id");
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
                log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Input incorrect id");
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
    chainLock.lock();
    if(verifyTransaction(tx))
    {
        if(transactions->get(tx.id)["id"].asString() == tx.id)
        {
            //Transaction has already been submitted and verified
            log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction that has already been verified");
            chainLock.unlock();
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
                chainLock.unlock();
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction we already know about");
                chainLock.unlock();
                return true;
            }
        }
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to verify transaction");
        chainLock.unlock();
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
    returning["confirmingBlock"] = tx.confirmingBlock;

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
    returning["spendData"] = Output.spendData;
    returning["creationTx"] = Output.creationTx;

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
    chainLock.lock();
    //Check block does not already exist
    if(blocks->get(newBlock.id)["id"].asString() == newBlock.id && blocks->get(newBlock.id)["mainChain"].asBool())
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block already exists");
        chainLock.unlock();
        return true;
    }

    const block previousBlock = getBlock(newBlock.previousBlockId);
    //Check the previous block exists
    if((previousBlock.id != newBlock.previousBlockId || newBlock.previousBlockId == "") && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Previous block does not exist");
        chainLock.unlock();
        return false;
    }

    if(newBlock.height != previousBlock.height + 1 && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block height is incorrect");
        chainLock.unlock();
        return false;
    }

    if((newBlock.sequenceNumber <= previousBlock.sequenceNumber || (newBlock.timestamp / blockTarget) != newBlock.sequenceNumber) && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Sequence number is incorrect");
        chainLock.unlock();
        return false;
    }

    //Check the id is correct
    if(calculateBlockId(newBlock) != newBlock.id)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block id is incorrect");
        chainLock.unlock();
        return false;
    }

    //Check that the timestamp is realistic
    if(newBlock.timestamp < previousBlock.timestamp && !genesisBlock)
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Timestamp is unrealistic");
        chainLock.unlock();
        return false;
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);
    if(now < newBlock.timestamp) {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Timestamp is too early");
        chainLock.unlock();
        return false;
    }

    if(!genesisBlock)
    {
        if(getVerifier(newBlock) != newBlock.publicKey) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Not from a valid verifier");
            chainLock.unlock();
            return false;
        }
    }

    CryptoKernel::Crypto crypto;
    crypto.setPublicKey(newBlock.publicKey);
    if(!crypto.verify(newBlock.id, newBlock.signature)) {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Invalid block signature");
        chainLock.unlock();
        return false;
    }

    bool onlySave = false;

    if(newBlock.previousBlockId != chainTipId && !genesisBlock)
    {
        //This block does not directly lead on from last block
        //Check if the verifier should've come before the current tip
        //If so, reorg, otherwise ignore it
        const block tip = getBlock("tip");
        if(newBlock.sequenceNumber < tip.sequenceNumber && newBlock.height >= tip.height)
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Forking the chain");
            std::string originalTip = tip.id;
            reorgChain(newBlock.previousBlockId);
        }
        else
        {
            log->printf(LOG_LEVEL_WARN, "blockchain::submitBlock(): Chain has a lower PoW than current chain");
            onlySave = true;
        }
    }

    if(!onlySave)
    {
        uint64_t fees = 0;
        //Verify Transactions
        std::vector<CryptoKernel::Blockchain::transaction>::iterator it;
        for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
        {
            (*it).confirmingBlock = newBlock.id;
            if(!submitTransaction((*it)))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Transaction could not be submitted");
                chainLock.unlock();
                return false;
            }
            fees += calculateTransactionFee((*it));
        }

        if(!newBlock.coinbaseTx.inputs.empty())
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase tx has inputs");
            chainLock.unlock();
            return false;
        }
        else if(newBlock.coinbaseTx.id != "")
        {
            uint64_t outputTotal = 0;
            std::vector<output>::iterator it2;
            for(it2 = newBlock.coinbaseTx.outputs.begin(); it2 < newBlock.coinbaseTx.outputs.end(); it2++)
            {
                (*it2).creationTx = newBlock.coinbaseTx.id;
                if((*it2).publicKey != cbKey)
                {
                    //Invalid key
                    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase public key is invalid");
                    chainLock.unlock();
                    return false;
                }
                outputTotal += (*it2).value;
            }

            if(outputTotal != fees)
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase output exceeds fees");
                chainLock.unlock();
                return false;
            }

            newBlock.coinbaseTx.confirmingBlock = newBlock.id;
            if(verifyTransaction(newBlock.coinbaseTx, true))
            {
                confirmTransaction(newBlock.coinbaseTx, true);
            }
            else
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Could not verify coinbase transaction");
                chainLock.unlock();
                return false;
            }
        }

        //Move transactions from unconfirmed to confirmed and add transaction utxos to db
        for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
        {
            if(!confirmTransaction((*it)))
            {
                reindexChain(chainTipId);
                chainLock.unlock();
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
        blocks->store("height_" + std::to_string(newBlock.height), Json::Value(newBlock.id));
    }

    blocks->store(newBlock.id, blockToJson(newBlock));

    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): successfully submitted block: " + CryptoKernel::Storage::toString(blocks->get(newBlock.id)));

    checkRep();

    chainLock.unlock();

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

    buffer << Block.previousBlockId << Block.timestamp << Block.height << Block.publicKey << Block.sequenceNumber;

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

    if(Block.coinbaseTx.id != "") {
        returning["coinbaseTx"] = transactionToJson(Block.coinbaseTx);
    }

    if(!PoW)
    {
        returning["mainChain"] = Block.mainChain;
    }

    returning["signature"] = Block.signature;
    returning["publicKey"] = Block.publicKey;
    returning["sequenceNumber"] = static_cast<unsigned long long int>(Block.sequenceNumber);

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
        (*it).creationTx = tx.id;
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

    for(unsigned int i = 0; i < Block["transactions"].size(); i++)
    {
        returning.transactions.push_back(jsonToTransaction(Block["transactions"][i]));
    }

    returning.coinbaseTx = jsonToTransaction(Block["coinbaseTx"]);
    returning.mainChain = Block["mainChain"].asBool();
    returning.publicKey = Block["publicKey"].asString();
    returning.signature = Block["signature"].asString();
    returning.sequenceNumber = Block["sequenceNumber"].asUInt64();

    return returning;
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::jsonToTransaction(Json::Value tx)
{
    transaction returning;

    returning.id = tx["id"].asString();
    returning.timestamp = tx["timestamp"].asUInt64();

    returning.confirmingBlock = tx["confirmingBlock"].asString();

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
    returning.spendData = Output["spendData"];
    returning.value = Output["value"].asUInt64();
    returning.nonce = Output["nonce"].asUInt64();
    returning.creationTx = Output["creationTx"].asString();

    return returning;
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

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::generateMiningBlock(std::string publicKey)
{
    chainLock.lock();
    block returning;

    returning.transactions = getUnconfirmedTransactions();
    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    returning.timestamp = now;
    returning.sequenceNumber = now / blockTarget;
    returning.publicKey = publicKey;

    block previousBlock;
    previousBlock = getBlock("tip");

    returning.height = previousBlock.height + 1;
    returning.previousBlockId = previousBlock.id;

    output toMe;
    toMe.value = 0;

    std::vector<transaction>::iterator it;
    for(it = returning.transactions.begin(); it < returning.transactions.end(); it++)
    {
        toMe.value += calculateTransactionFee((*it));
    }
    toMe.publicKey = cbKey;

    std::default_random_engine generator(now);
    std::uniform_int_distribution<unsigned int> distribution(0, UINT_MAX);
    toMe.nonce = distribution(generator);

    toMe.id = calculateOutputId(toMe);

    transaction coinbaseTx;
    coinbaseTx.outputs.push_back(toMe);
    coinbaseTx.timestamp = now;
    coinbaseTx.id = calculateTransactionId(coinbaseTx);

    if(coinbaseTx.outputs[0].value > 0) {
        returning.coinbaseTx = coinbaseTx;
    }

    returning.id = calculateBlockId(returning);

    chainLock.unlock();

    return returning;
}

std::vector<CryptoKernel::Blockchain::output> CryptoKernel::Blockchain::getUnspentOutputs(std::string publicKey)
{
    chainLock.lock();
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

    chainLock.unlock();

    return returning;
}
std::string CryptoKernel::Blockchain::calculateOutputSetId(const std::vector<output> outputs)
{
    std::vector<output> orderedOutputs = outputs;

    std::sort(orderedOutputs.begin(), orderedOutputs.end(), [](CryptoKernel::Blockchain::output first, CryptoKernel::Blockchain::output second){
        return CryptoKernel::Math::hex_greater(second.id, first.id);
    });

    std::string returning = "";
    CryptoKernel::Crypto crypto;
    for(CryptoKernel::Blockchain::output output : orderedOutputs)
    {
        returning = crypto.sha256(returning + output.id);
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

    blocks->erase("height_" + std::to_string(tip.height));
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

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::getTransaction(const std::string id)
{
    return CryptoKernel::Blockchain::jsonToTransaction(transactions->get(id));
}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getTransactionsByPubKeys(const std::set<std::string> pubKeys) {
    chainLock.lock();

    std::set<transaction> returning;

    block currentBlock;
    currentBlock.previousBlockId = getBlock("tip").id;
    do {
        currentBlock = getBlock(currentBlock.previousBlockId);
        for(const transaction tx : currentBlock.transactions) {
            bool found = false;
            for(const output output : tx.outputs) {
                if(pubKeys.find(output.publicKey) != pubKeys.end()) {
                    returning.insert(tx);
                    found = true;
                    break;
                }
            }

            if(!found) {
                for(const output input : tx.inputs) {
                    if(pubKeys.find(input.publicKey) != pubKeys.end()) {
                        returning.insert(tx);
                        break;
                    }
                }
            }
        }
    } while(currentBlock.id != genesisBlockId);

    chainLock.unlock();

    return returning;
}

std::string CryptoKernel::Blockchain::getVerifier(const block& thisBlock) {
    const uint64_t verifierId = thisBlock.sequenceNumber % verifiers.size();

    return *std::next(verifiers.begin(), verifierId);
}
