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

CryptoKernel::Blockchain::Blockchain(CryptoKernel::Log* GlobalLog)
{
    status = false;
    blockdb.reset(new CryptoKernel::Storage("./blockdb"));
    blocks.reset(new CryptoKernel::Storage::Table("blocks"));
    transactions.reset(new CryptoKernel::Storage::Table("transactions"));
    utxos.reset(new CryptoKernel::Storage::Table("utxos"));
    stxos.reset(new CryptoKernel::Storage::Table("stxos"));
    inputs.reset(new CryptoKernel::Storage::Table("inputs"));
    candidates.reset(new CryptoKernel::Storage::Table("candidates"));
    log = GlobalLog;
}

bool CryptoKernel::Blockchain::loadChain(CryptoKernel::Consensus* consensus)
{
    chainLock.lock();
    this->consensus = consensus;
    std::unique_ptr<Storage::Transaction> dbTransaction(blockdb->begin());
    const bool tipExists = blocks->get(dbTransaction.get(), "tip").isObject();
    dbTransaction->abort();
    if(!tipExists)
    {
        emptyDB();
        bool newGenesisBlock = false;
        std::ifstream t("genesisblock.json");
        if(!t.is_open())
        {
            log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to open genesis block file");
            newGenesisBlock = true;
        } else {
            std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

            block genesisBlock(CryptoKernel::Storage::toJson(buffer));

            if(submitBlock(genesisBlock, true))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain(): Successfully imported genesis block");
            }
            else
            {
                log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to import genesis block");
                newGenesisBlock = true;
            }

            t.close();
        }

        if(newGenesisBlock) {
            log->printf(LOG_LEVEL_INFO, "blockchain(): Generating new genesis block");
            CryptoKernel::Crypto crypto(true);
            const block Block = generateVerifyingBlock(crypto.getPublicKey());

            if(!submitBlock(Block, true)) {
                log->printf(LOG_LEVEL_ERR, "blockchain(): Failed to import new genesis block");
            }

            std::ofstream f;
            f.open("genesisblock.json");
            f << CryptoKernel::Storage::toString(Block.toJson(), true);
            f.close();
        }
    }

    const block genesisBlock = getBlockByHeight(1);
    genesisBlockId = genesisBlock.getId();

    status = true;

    chainLock.unlock();

    return true;
}

CryptoKernel::Blockchain::~Blockchain()
{
    chainLock.lock();
}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getUnconfirmedTransactions()
{
    chainLock.lock();
    const std::set<CryptoKernel::Blockchain::transaction> returning = unconfirmedTransactions;
    chainLock.unlock();

    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(Storage::Transaction* transaction, const std::string id)
{
    const dbBlock block = getBlockDB(transaction, id);

    return buildBlock(transaction, block);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(Storage::Transaction* transaction, const uint64_t height)
{
    const std::string id = blocks->get(transaction, std::to_string(height), 0).asString();
    return getBlock(transaction, id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(const std::string id)
{
    chainLock.lock();
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());
    const block returning = getBlock(tx.get(), id);
    chainLock.unlock();
    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(const uint64_t height)
{
    chainLock.lock();
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());
    const block returning = getBlockByHeight(tx.get(), height);
    chainLock.unlock();
    return returning;
}

uint64_t CryptoKernel::Blockchain::getBalance(std::string publicKey)
{
    chainLock.lock();
    uint64_t total = 0;

    if(status)
    {
        CryptoKernel::Storage::Table::Iterator *it = new CryptoKernel::Storage::Table::Iterator(utxos.get(), blockdb.get());
        for(it->SeekToFirst(); it->Valid(); it->Next())
        {
            const dbOutput out = dbOutput(it->value());
            const Json::Value data = out.getData();

            if(data["publicKey"].asString() == publicKey && data["contract"].empty()) {
                total += out.getValue();
            }
        }
        delete it;
    }

    chainLock.unlock();

    return total;
}

bool CryptoKernel::Blockchain::verifyTransaction(Storage::Transaction* dbTransaction, const transaction& tx, const bool coinbaseTx)
{
    if(transactions->get(dbTransaction, tx.getId().toString()).isObject())
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx already exists");
        return false;
    }

    uint64_t inputTotal = 0;
    uint64_t outputTotal = 0;

    for(const output& out : tx.getOutputs()) {
        if(utxos->get(dbTransaction, out.getId().toString()).isObject()) {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output already in UTXO set");
            //Duplicate output
            return false;
        }

        outputTotal += out.getValue();
    }

    const CryptoKernel::BigNum outputHash = tx.getOutputSetId();

    for(const input& inp : tx.getInputs()) {
        const Json::Value outJson = utxos->get(dbTransaction, inp.getOutputId().toString());
        if(!outJson.isObject()) {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output has already been spent");
            return false;
        }

        const dbOutput out = dbOutput(outJson);
        inputTotal += out.getValue();

        const Json::Value outData = out.getData();
        if(!outData["publicKey"].empty() && outData["contract"].empty()) {
            const Json::Value spendData = inp.getData();
            if(spendData["signature"].empty()) {
                log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Could not verify input signature");
                return false;
            }

            CryptoKernel::Crypto crypto;
            crypto.setPublicKey(outData["publicKey"].asString());
            if(!crypto.verify(out.getId().toString() + outputHash.toString(), spendData["signature"].asString()))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Could not verify input signature");
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
    if(!lvm.evaluateValid(dbTransaction, tx))
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Script returned false");
        return false;
    }

    if(!consensus->verifyTransaction(dbTransaction, tx)) {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Could not verify custom rules");
        return false;
    }

    return true;
}

bool CryptoKernel::Blockchain::submitTransaction(const transaction tx) {
    chainLock.lock();
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    const bool result = submitTransaction(dbTx.get(), tx);
    if(result) {
        dbTx->commit();
    }
    chainLock.unlock();
    return result;
}

bool CryptoKernel::Blockchain::submitTransaction(Storage::Transaction* dbTx, transaction tx)
{
    chainLock.lock();
    if(verifyTransaction(dbTx, tx))
    {
        if(consensus->submitTransaction(dbTx, tx)) {
            //Transaction has not already been verified
            bool found = false;

            //Check if transaction is already in the unconfirmed vector
            for(const transaction& utx : unconfirmedTransactions)
            {
                found = (utx.getId() == tx.getId());
                if(found)
                {
                    break;
                }
            }

            if(!found)
            {
                unconfirmedTransactions.insert(tx);
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
        } else {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to submit transaction to consensus method");
            chainLock.unlock();
            return false;
        }
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to verify transaction");
        chainLock.unlock();
        return false;
    }
}

bool CryptoKernel::Blockchain::submitBlock(Storage::Transaction* dbTx, block newBlock, bool genesisBlock)
{
    chainLock.lock();
    const std::string idAsString = newBlock.getId().toString();
    //Check block does not already exist
    if(blocks->get(dbTx, idAsString).isObject())
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block already exists");
        chainLock.unlock();
        return true;
    }

    Json::Value previousBlockJson = blocks->get(dbTx, newBlock.getPreviousBlockId().toString());
    uint64_t blockHeight = 1;

    bool onlySave = false;

    if(!genesisBlock) {
        if(!previousBlockJson.isObject()) {
            previousBlockJson = candidates->get(dbTx, newBlock.getPreviousBlockId().toString());
            if(!previousBlockJson.isObject()) {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Previous block does not exist");
                chainLock.unlock();
                return false;
            }

            const block previousBlock = block(previousBlockJson);
            previousBlockJson = dbBlock(previousBlock, 0).toJson();
        }

        const dbBlock previousBlock = dbBlock(previousBlockJson);

        //Check that the timestamp is realistic
        if(newBlock.getTimestamp() < previousBlock.getTimestamp())
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Timestamp is unrealistic");
            chainLock.unlock();
            return false;
        }

        if(!consensus->checkConsensusRules(dbTx, newBlock, previousBlock)) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Consensus rules cannot verify this block");
            chainLock.unlock();
            return false;
        }

        const dbBlock tip = getBlockDB(dbTx, "tip");
        if(previousBlock.getId() != tip.getId())
        {
            //This block does not directly lead on from last block
            //Check if the verifier should've come before the current tip
            //If so, reorg, otherwise ignore it
            if(consensus->isBlockBetter(dbTx, newBlock, tip))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Forking the chain");
                if(!reorgChain(dbTx, previousBlock.getId())) {
                    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Alternative chain is not valid");
                    return false;
                }

                blockHeight = getBlockDB(dbTx, "tip").getHeight() + 1;
            }
            else
            {
                log->printf(LOG_LEVEL_WARN, "blockchain::submitBlock(): Chain has less verifier backing than current chain");
                onlySave = true;
            }
        } else {
            blockHeight = tip.getHeight() + 1;
        }
    }

    if(!onlySave)
    {
        uint64_t fees = 0;
        //Verify Transactions
        for(const transaction& tx : newBlock.getTransactions())
        {
            if(!verifyTransaction(dbTx, tx))
            {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Transaction could not be verified");
                chainLock.unlock();
                return false;
            }
            fees += calculateTransactionFee(dbTx, tx);
        }

        if(!verifyTransaction(dbTx, newBlock.getCoinbaseTx(), true))
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase transaction could not be verified");
            chainLock.unlock();
            return false;
        }

        uint64_t outputTotal = 0;
        for(const output& out : newBlock.getCoinbaseTx().getOutputs())
        {
            outputTotal += out.getValue();
        }

        if(outputTotal != fees + getBlockReward(blockHeight))
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase output is not the correct value");
            chainLock.unlock();
            return false;
        }

        if(!consensus->submitBlock(dbTx, newBlock)) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Consensus submitBlock callback returned false");
            chainLock.unlock();
            return false;
        }

        confirmTransaction(dbTx, newBlock.getCoinbaseTx(), newBlock.getId(), true);

        //Move transactions from unconfirmed to confirmed and add transaction utxos to db
        for(const transaction& tx : newBlock.getTransactions()) {
            confirmTransaction(dbTx, tx, newBlock.getId());
        }
    }

    if(onlySave) {
        candidates->put(dbTx, newBlock.getId().toString(), newBlock.toJson());
    } else {
        const dbBlock toSave = dbBlock(newBlock, blockHeight);
        const Json::Value blockAsJson = toSave.toJson();
        candidates->erase(dbTx, idAsString);
        blocks->put(dbTx, "tip", blockAsJson);
        blocks->put(dbTx, std::to_string(blockHeight), Json::Value(idAsString), 0);
        blocks->put(dbTx, idAsString, blockAsJson);
    }

    if(genesisBlock) {
        genesisBlockId = newBlock.getId();
    }

    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): successfully submitted block: " + CryptoKernel::Storage::toString(blocks->get(dbTx, idAsString), true));

    chainLock.unlock();

    return true;
}

void CryptoKernel::Blockchain::confirmTransaction(Storage::Transaction* dbTransaction, const transaction& tx, const BigNum& confirmingBlock, const bool coinbaseTx)
{
    //Execute custom transaction rules callback
    if(!consensus->confirmTransaction(dbTransaction, tx)) {
        log->printf(LOG_LEVEL_ERR, "Consensus rules failed to confirm transaction");
    }

    //"Spend" UTXOs
    for(const input& inp : tx.getInputs()) {
        const std::string outputId = inp.getOutputId().toString();
        const Json::Value utxo = utxos->get(dbTransaction, outputId);
        stxos->put(dbTransaction, outputId, utxo);
        utxos->erase(dbTransaction, outputId);

        inputs->put(dbTransaction, inp.getId().toString(), dbInput(&inp).toJson());
    }

    //Add new outputs to UTXOs
    for(const output& out : tx.getOutputs()) {
        utxos->put(dbTransaction, out.getId().toString(), dbOutput(&out, tx.getId()).toJson());
    }

    //Commit transaction
    transactions->put(dbTransaction, tx.getId().toString(), Blockchain::dbTransaction(tx, confirmingBlock, coinbaseTx).toJson());

    //Remove transaction from unconfirmed transactions vector
    unconfirmedTransactions.erase(tx);
}

bool CryptoKernel::Blockchain::reorgChain(Storage::Transaction* dbTransaction, const std::string newTipId)
{
    std::stack<block> blockList;

    //Find common fork block
    block currentBlock = getBlock(dbTransaction, newTipId);
    while(!currentBlock.mainChain && currentBlock.previousBlockId != "")
    {
        blockList.push(currentBlock);
        std::string id = currentBlock.previousBlockId;
        currentBlock = getBlock(dbTransaction, id);
        if(currentBlock.id != id)
        {
            return false;
        }
    }

    if(blocks->get(dbTransaction, currentBlock.id)["id"].asString() != currentBlock.id)
    {
        return false;
    }

    //Reverse blocks to that point
    while(getBlock(dbTransaction, "tip").id != currentBlock.id)
    {
        reverseBlock(dbTransaction);
    }

    //Submit new blocks
    while(blockList.size() > 0)
    {
        if(!submitBlock(dbTransaction, blockList.top()))
        {
            log->printf(LOG_LEVEL_WARN, "blockchain::reorgChain(): Failed to reapply original chain after new chain failed to verify");

            return false;
        }
        blockList.pop();
    }

    return true;
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

uint64_t CryptoKernel::Blockchain::calculateTransactionFee(Storage::Transaction* dbTx, transaction tx)
{
    uint64_t inputTotal = 0;
    uint64_t outputTotal = 0;

    for(const output& out : tx.getOutputs()) {
        outputTotal += out.getValue();
    }

    for(const input& inp : tx.getInputs()) {
        const dbOutput out = dbOutput(utxos->get(dbTx, inp.getOutputId().toString()));
        inputTotal += out.getValue();
    }

    return inputTotal - outputTotal;
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::generateVerifyingBlock(std::string publicKey)
{
    chainLock.lock();
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    block returning;

    returning.transactions = getUnconfirmedTransactions();
    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    returning.timestamp = now;

    block previousBlock;
    previousBlock = getBlock(dbTx.get(), "tip");

    returning.height = previousBlock.height + 1;
    returning.previousBlockId = previousBlock.id;

    output toMe;
    toMe.value = getBlockReward(returning.height);

    std::vector<transaction>::iterator it;
    for(it = returning.transactions.begin(); it < returning.transactions.end(); it++)
    {
        toMe.value += calculateTransactionFee((*it));
    }
    toMe.publicKey = getCoinbaseOwner(publicKey);

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

    returning.consensusData = consensus->generateConsensusData(dbTx.get(), returning, publicKey);

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
        CryptoKernel::Storage::Table::Iterator* it = new Storage::Table::Iterator(utxos.get(), blockdb.get());
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

void CryptoKernel::Blockchain::reverseBlock(Storage::Transaction* dbTransaction)
{
    block tip = getBlock(dbTransaction, "tip");

    std::vector<output>::iterator it2;
    for(it2 = tip.coinbaseTx.outputs.begin(); it2 < tip.coinbaseTx.outputs.end(); it2++)
    {
        utxos->erase(dbTransaction, (*it2).id);
    }

    std::vector<transaction>::iterator it;
    for(it = tip.transactions.begin(); it < tip.transactions.end(); it++)
    {
        for(it2 = (*it).outputs.begin(); it2 < (*it).outputs.end(); it2++)
        {
            utxos->erase(dbTransaction, (*it2).id);
        }

        for(it2 = (*it).inputs.begin(); it2 < (*it).inputs.end(); it2++)
        {
            output toSave = (*it2);
            toSave.signature = "";
            utxos->put(dbTransaction, toSave.id, outputToJson(toSave));
        }

        transactions->erase(dbTransaction, (*it).id);

        if(!submitTransaction(dbTransaction, *it))
        {
            log->printf(LOG_LEVEL_ERR, "Blockchain::reverseBlock(): previously moved transaction is now invalid");
        }
    }

    blocks->erase(dbTransaction, std::to_string(tip.height), 0);
    blocks->put(dbTransaction, "tip", blockToJson(getBlock(dbTransaction, tip.previousBlockId)));

    tip.mainChain = false;
    blocks->put(dbTransaction, tip.id, blockToJson(tip));
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::getTransaction(Storage::Transaction* transaction, const std::string id)
{

    return CryptoKernel::Blockchain::jsonToTransaction(transactions->get(transaction, id));
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

void CryptoKernel::Blockchain::emptyDB() {
    blockdb.reset();
    CryptoKernel::Storage::destroy("./blockdb");
    blockdb.reset(new CryptoKernel::Storage("./blockdb"));
}

CryptoKernel::Storage::Transaction* CryptoKernel::Blockchain::getTxHandle() {
    chainLock.lock();
    Storage::Transaction* dbTx = blockdb->begin();
    chainLock.unlock();
    return dbTx;
}
