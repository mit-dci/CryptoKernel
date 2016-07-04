#include <ctime>
#include <sstream>
#include <algorithm>
#include <stack>

#include "blockchain.h"
#include "crypto.h"

CryptoKernel::Blockchain::Blockchain()
{
    transactions = new CryptoKernel::Storage("./transactiondb");
    blocks = new CryptoKernel::Storage("./blockdb");
    utxos = new CryptoKernel::Storage("./utxodb");

    chainTipId = blocks->get("tip")["id"].asString();
}

CryptoKernel::Blockchain::~Blockchain()
{
    delete transactions;
    delete blocks;
    delete utxos;
}

double CryptoKernel::Blockchain::getBlockReward()
{
    return 50;
}

bool CryptoKernel::Blockchain::isCoinbaseTransaction(transaction tx)
{
    if(!tx.inputs.empty())
    {
        return false;
    }

    double outputTotal = 0;

    std::vector<output>::iterator it;
    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        outputTotal += (*it).value;
    }

    if(outputTotal != getBlockReward())
    {
        return false;
    }

    return true;
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

    return total;
}

bool CryptoKernel::Blockchain::verifyTransaction(transaction tx, bool coinbaseTx)
{
    if(calculateTransactionId(tx) != tx.id)
    {
        return false;
    }

    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    if(tx.timestamp < (now - 5 * 60 * 60))
    {
        return false;
    }

    double inputTotal = 0;
    double outputTotal = 0;
    std::vector<output>::iterator it;

    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        inputTotal += (*it).value;
        CryptoKernel::Crypto crypto;
        crypto.setPublicKey((*it).publicKey);
        if(!crypto.verify((*it).id, (*it).signature))
        {
            return false;
        }
        else
        {
            //Check if input has already been spent
            if(utxos->get((*it).id)["id"] == "")
            {
                return false;
            }
        }
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        outputTotal += (*it).value;
    }

    if(!coinbaseTx)
    {
        if(outputTotal > inputTotal)
        {
            return false;
        }

        double fee = inputTotal - outputTotal;
        if(fee < getTransactionFee(tx) * 0.5)
        {
            return false;
        }
    }

    return true;
}

bool CryptoKernel::Blockchain::submitTransaction(transaction tx)
{
    if(verifyTransaction(tx) && !isCoinbaseTransaction(tx))
    {
        if(transactions->get(tx.id)["id"] != "")
        {
            //Transaction has already been submitted and verified
            return false;
        }
        else
        {
            //Transaction has not already been verified

            //Check if transaction is already in the unconfirmed vector
            std::vector<transaction>::iterator it;
            it = std::find_if(unconfirmedTransactions.begin(), unconfirmedTransactions.end(), [&](const transaction & utx)
            {
                return (utx.id == tx.id);
            });

            if(it->id == "")
            {
                unconfirmedTransactions.push_back(tx);
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                return true;
            }
        }
    }
    else
    {
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
        returning["inputs"].append(outputToJson((*it)));
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

    buffer << Output.publicKey << Output.value << CryptoKernel::Storage::toString(Output.data);

    CryptoKernel::Crypto crypto;
    return crypto.sha256(buffer.str());
}

bool CryptoKernel::Blockchain::submitBlock(block newBlock)
{
    //Check block does not already exist
    if(blocks->get(newBlock.id)["id"] != "")
    {
        return false;
    }

    //Check the previous block exists
    if(blocks->get(newBlock.previousBlockId)["id"] == "")
    {
        return false;
    }

    //Check target
    if(newBlock.target != calculateTarget(newBlock.previousBlockId))
    {
        return false;
    }

    //Check proof of work
    if(!hex_greater(newBlock.target, newBlock.PoW))
    {
        return false;
    }

    //Check total work
    if(newBlock.totalWork != addHex(newBlock.PoW, jsonToBlock(blocks->get(newBlock.previousBlockId)).totalWork))
    {
        return false;
    }

    bool needToReorg = false;

    if(newBlock.previousBlockId != chainTipId)
    {
        //This block does not directly lead on from last block
        //Check if its PoW is bigger than the longest chain
        //If so, reorg, otherwise ignore it
        if(hex_greater(jsonToBlock(blocks->get("tip")).totalWork, newBlock.totalWork))
        {
            needToReorg = true;
        }
        else
        {
            return false;
        }
    }

    //Check the id is correct
    if(calculateBlockId(newBlock) != newBlock.id)
    {
        return false;
    }

    //Check that the timestamp is realistic
    if(newBlock.timestamp < (jsonToBlock(blocks->get(newBlock.previousBlockId)).timestamp - 24 * 60 * 60))
    {
        return false;
    }

    double fees = 0;

    //Verify Transactions
    std::vector<transaction>::iterator it;
    for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
    {
        if(!submitTransaction((*it)))
        {
            return false;
        }
        fees += calculateTransactionFee((*it));
    }

    if(!newBlock.coinbaseTx.inputs.empty())
    {
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
            return false;
        }

        if(verifyTransaction(newBlock.coinbaseTx, true))
        {
            confirmTransaction(newBlock.coinbaseTx, true);
        }
        else
        {
            return false;
        }
    }

    //Move transactions from unconfirmed to confirmed and add transaction utxos to db
    for(it = newBlock.transactions.begin(); it < newBlock.transactions.end(); it++)
    {
        confirmTransaction((*it));
    }

    blocks->store(newBlock.id, blockToJson(newBlock));

    chainTipId = newBlock.id;
    blocks->store("tip", blockToJson(newBlock));

    if(needToReorg)
    {
        reorgChain(newBlock.id);
    }

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

    buffer << Block.previousBlockId << Block.timestamp;

    CryptoKernel::Crypto crypto;
    return crypto.sha256(buffer.str());
}

Json::Value CryptoKernel::Blockchain::blockToJson(block Block)
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

    returning["PoW"] = Block.PoW;
    returning["target"] = Block.target;
    returning["totalWork"] = Block.totalWork;

    return returning;
}

bool CryptoKernel::Blockchain::confirmTransaction(transaction tx, bool coinbaseTx)
{
    //Check if transaction is already confirmed
    if(transactions->get(tx.id)["id"] != "")
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
    it2 = std::remove_if(unconfirmedTransactions.begin(), unconfirmedTransactions.end(), [&](const transaction & utx)
    {
        return (utx.id == tx.id);
    });

    return true;
}

bool CryptoKernel::Blockchain::reorgChain(std::string newTipId)
{
    std::stack<block> blockList;

    chainTipId = "";

    delete transactions;
    CryptoKernel::Storage::destroy("./transactiondb");
    transactions = new CryptoKernel::Storage("./transactiondb");

    delete utxos;
    CryptoKernel::Storage::destroy("./utxodb");
    utxos = new CryptoKernel::Storage("./utxodb");

    block currentBlock = jsonToBlock(blocks->get(newTipId));
    while(currentBlock.previousBlockId != "")
    {
        blockList.push(currentBlock);
        currentBlock = jsonToBlock(blocks->get(currentBlock.previousBlockId));
    }

    delete blocks;
    CryptoKernel::Storage::destroy("./blockdb");
    blocks = new CryptoKernel::Storage("./blockdb");

    while(!blockList.empty())
    {
        submitBlock(currentBlock);
        currentBlock = blockList.top();
        blockList.pop();
    }

    submitBlock(currentBlock);

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

    return returning;
}

std::string CryptoKernel::Blockchain::calculateTarget(std::string previousBlockId)
{
    if(previousBlockId == "")
    {
        return "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    }
    else
    {
        uint64_t totalTime = 0;

        unsigned int i;
        block currentBlock = jsonToBlock(blocks->get(previousBlockId));
        for(i = 0; i < 200; i++)
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

        uint64_t blockTime = totalTime / i;
        uint64_t delta = blockTime - 150;

        std::stringstream buffer;

        std::string newTarget;

        if(delta > 0)
        {
            buffer << std::hex << delta;
            newTarget = addHex(jsonToBlock(blocks->get(previousBlockId)).target, buffer.str());
        }
        else if(delta < 0)
        {
            buffer << std::hex << -delta;
            newTarget = subtractHex(jsonToBlock(blocks->get(previousBlockId)).target, buffer.str());
        }
        else
        {
            newTarget = jsonToBlock(blocks->get(previousBlockId)).target;
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
