#include <ctime>
#include <sstream>
#include <algorithm>
#include <stack>
#include <queue>
#include <fstream>

#include "blockchain.h"
#include "crypto.h"

CryptoKernel::Blockchain::Blockchain()
{
    transactions = new CryptoKernel::Storage("./transactiondb");
    blocks = new CryptoKernel::Storage("./blockdb");
    utxos = new CryptoKernel::Storage("./utxodb");
    log = new CryptoKernel::Log("blockchain.log", true);

    chainTipId = blocks->get("tip")["id"].asString();
    if(chainTipId == "")
    {
        if(blocks->get(genesisBlockId)["id"].asString() != genesisBlockId)
        {
            std::ifstream t("genesisblock.txt");
            std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

            if(submitBlock(jsonToBlock(CryptoKernel::Storage::toJson(buffer))))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain(): Successfully imported genesis block");
            }
            else
            {
                log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to import genesis block");
            }
        }
    }

    reorgChain(chainTipId);
}

CryptoKernel::Blockchain::~Blockchain()
{
    delete transactions;
    delete blocks;
    delete utxos;
    delete log;
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getUnconfirmedTransactions()
{
    return unconfirmedTransactions;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(std::string id)
{
    return jsonToBlock(blocks->get(id));
}

double CryptoKernel::Blockchain::getBlockReward()
{
    return 50;
}

double CryptoKernel::Blockchain::getBalance(std::string publicKey)
{
    double total = 0;

    CryptoKernel::Storage::Iterator *it = utxos->newIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        if(it->value()["publicKey"] == publicKey)
        {
            total += it->value()["value"].asDouble();
        }
    }
    delete it;

    return total;
}

bool CryptoKernel::Blockchain::verifyTransaction(transaction tx, bool coinbaseTx)
{
    if(calculateTransactionId(tx) != tx.id)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): tx id is incorrect");
        return false;
    }

    if(transactions->get(tx.id)["id"].asString() == tx.id)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): tx already exists");
        return false;
    }

    /*time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    if(tx.timestamp < (now - 5 * 60 * 60))
    {
        return false;
    }*/

    double inputTotal = 0;
    double outputTotal = 0;
    std::vector<output>::iterator it;

    std::string outputHash = calculateOutputSetId(tx.outputs);

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        if(utxos->get((*it).id)["id"] == (*it).id || (*it).value < 0.00000001)
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): Duplicate output in tx");
            //Duplicate output
            return false;
        }
        outputTotal += (*it).value;
    }

    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        inputTotal += (*it).value;
        CryptoKernel::Crypto crypto;
        crypto.setPublicKey((*it).publicKey);
        if(!crypto.verify((*it).id + outputHash, (*it).signature) || (*it).value <= 0)
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): Could not verify input signature");
            return false;
        }
        else
        {
            //Check if input has already been spent
            std::string id = utxos->get((*it).id)["id"].asString();
            if(id != (*it).id)
            {
                log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): Output has already been spent");
                return false;
            }
        }
    }

    if(!coinbaseTx)
    {
        if(outputTotal > inputTotal)
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): The output total is greater than the input total");
            return false;
        }

        double fee = inputTotal - outputTotal;
        if(fee < getTransactionFee(tx) * 0.5)
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::verifyTransaction(): tx fee is too low");
            return false;
        }
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
            log->printf(LOG_LEVEL_ERR, "blockchain::submitTransaction(): Received transaction that has already been verified");
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
                log->printf(LOG_LEVEL_ERR, "blockchain::submitTransaction(): Received transaction we didn't already know about");
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                log->printf(LOG_LEVEL_ERR, "blockchain::submitTransaction(): Received transaction we already know about");
                return true;
            }
        }
    }
    else
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitTransaction(): Failed to verify transaction");
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
    returning["value"] = Output.value;
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
    if(blocks->get(newBlock.id)["id"].asString() == newBlock.id)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Block already exists");
        return false;
    }

    //Check the previous block exists
    if((blocks->get(newBlock.previousBlockId)["id"].asString() != newBlock.previousBlockId || newBlock.previousBlockId != "") && !genesisBlock)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Previous block does not exist");
        return false;
    }

    //Check target
    if(newBlock.target != calculateTarget(newBlock.previousBlockId))
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Block target is incorrect");
        return false;
    }

    //Check proof of work
    if(!hex_greater(newBlock.target, newBlock.PoW) || calculatePoW(newBlock) != newBlock.PoW)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Proof of work is incorrect");
        return false;
    }

    //Check total work
    if(newBlock.totalWork != addHex(newBlock.PoW, jsonToBlock(blocks->get(newBlock.previousBlockId)).totalWork) && !genesisBlock)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Total work is incorrect");
        return false;
    }

    if(newBlock.previousBlockId != chainTipId && !genesisBlock)
    {
        //This block does not directly lead on from last block
        //Check if its PoW is bigger than the longest chain
        //If so, reorg, otherwise ignore it
        if(hex_greater(jsonToBlock(blocks->get("tip")).totalWork, newBlock.totalWork))
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Forking the chain");
            reorgChain(newBlock.previousBlockId);
        }
        else
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Chain has a lower PoW than current chain");
            return false;
        }
    }

    //Check the id is correct
    if(calculateBlockId(newBlock) != newBlock.id)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Block id is incorrect");
        return false;
    }

    //Check that the timestamp is realistic
    /*if(newBlock.timestamp < (jsonToBlock(blocks->get(newBlock.previousBlockId)).timestamp - 24 * 60 * 60) && !genesisBlock)
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Timestamp is unrealistic");
        return false;
    }*/

    double fees = 0;

    std::vector<transaction>::iterator it;
    for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
    {
        if(!verifyTransaction(*it))
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Transaction could not be verified");
            return false;
        }
    }

    //Verify Transactions
    for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
    {
        if(!submitTransaction((*it)))
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Transaction could not be submitted");
            return false;
        }
        fees += calculateTransactionFee((*it));
    }

    if(!newBlock.coinbaseTx.inputs.empty())
    {
        log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Coinbase tx has inputs");
        return false;
    }
    else
    {
        double outputTotal = 0;
        std::vector<output>::iterator it2;
        for(it2 = newBlock.coinbaseTx.outputs.begin(); it2 < newBlock.coinbaseTx.outputs.end(); it2++)
        {
            outputTotal += (*it2).value;
        }

        if(outputTotal != fees + getBlockReward())
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Coinbase output exceeds limit");
            return false;
        }

        if(verifyTransaction(newBlock.coinbaseTx, true))
        {
            confirmTransaction(newBlock.coinbaseTx, true);
        }
        else
        {
            log->printf(LOG_LEVEL_ERR, "blockchain::submitBlock(): Could not verify coinbase transaction");
            return false;
        }
    }

    //Move transactions from unconfirmed to confirmed and add transaction utxos to db
    for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
    {
        if(!confirmTransaction((*it)))
        {
            reorgChain(chainTipId);
            return false;
        }
    }

    blocks->store(newBlock.id, blockToJson(newBlock));

    chainTipId = newBlock.id;
    blocks->store("tip", blockToJson(newBlock));

    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): successfully submitted block: " + CryptoKernel::Storage::toString(blockToJson(newBlock)));

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

    buffer << Block.previousBlockId << Block.timestamp;

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

bool CryptoKernel::Blockchain::reorgChain(std::string newTipId)
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
        log->printf(LOG_LEVEL_WARN, "blockchain::reorgChain(): Chain has incorrect genesis block");
        return false;
    }

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
        submitBlock(currentBlock);
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
    returning.value = Output["value"].asDouble();
    returning.nonce = Output["nonce"].asUInt64();

    return returning;
}

std::string CryptoKernel::Blockchain::calculateTarget(std::string previousBlockId)
{
    if(previousBlockId == "")
    {
        return "0000000000ffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    }
    else
    {
        uint64_t totalTime = 0;

        uint64_t i;
        block currentBlock = jsonToBlock(blocks->get(previousBlockId));
        if(currentBlock.previousBlockId != "")
        {
            for(i = 1; i < 200; i++)
            {
                block nextBlock;
                if(currentBlock.previousBlockId != "")
                {
                    nextBlock = jsonToBlock(blocks->get(currentBlock.previousBlockId));
                }
                else
                {
                    break;
                }
                uint64_t timeDifference = currentBlock.timestamp - nextBlock.timestamp;
                totalTime += timeDifference;
                currentBlock = nextBlock;
            }
        }
        else
        {
            return "0000000000ffffffffffffffffffffffffffffffffffffffffffffffffffffff";
        }

        uint64_t blockTime = totalTime / i;
        int64_t delta = blockTime - 150;

        std::stringstream buffer;

        std::string newTarget;

        block previousBlock = jsonToBlock(blocks->get(previousBlockId));

        if(delta > 0)
        {
            for(int64_t i2 = 0; i2 < delta / 2; i2++)
            {
                if(hex_greater(previousBlock.target, buffer.str() + "1"))
                {
                    break;
                }
                buffer << "1";
            }
            newTarget = addHex(previousBlock.target, buffer.str());
        }
        else if(delta < 0)
        {
            for(int64_t i2 = 0; i2 < -delta / 2; i2++)
            {
                if(!hex_greater(previousBlock.target, buffer.str() + "1"))
                {
                    break;
                }
                buffer << "1";
            }
            newTarget = subtractHex(previousBlock.target, buffer.str());
        }
        else
        {
            newTarget = previousBlock.target;
        }

        return newTarget;
    }
}

double CryptoKernel::Blockchain::getTransactionFee(transaction tx)
{
    double fee = 0;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        if((*it).value < 0.01)
        {
            fee += 0.001;
        }

        fee += CryptoKernel::Storage::toString((*it).data).size() * 0.00001;
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        if((*it).value < 0.01)
        {
            fee += 0.001;
        }

        fee += CryptoKernel::Storage::toString((*it).data).size() * 0.00001;
    }

    return fee;
}

double CryptoKernel::Blockchain::calculateTransactionFee(transaction tx)
{
    double inputTotal = 0;
    double outputTotal = 0;
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
    std::string data = CryptoKernel::Storage::toString(blockToJson(Block, true));

    CryptoKernel::Crypto crypto;
    return crypto.sha256(data);
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
    toMe.value = getBlockReward();

    std::vector<transaction>::iterator it;
    for(it = returning.transactions.begin(); it < returning.transactions.end(); it++)
    {
        toMe.value += calculateTransactionFee((*it));
    }
    toMe.publicKey = publicKey;
    toMe.nonce = now;

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

    CryptoKernel::Storage::Iterator *it = utxos->newIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        if(it->value()["publicKey"] == publicKey)
        {
            returning.push_back(jsonToOutput(it->value()));
        }
    }
    delete it;

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
            if(!hex_greater((*it).id, lowestId))
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
