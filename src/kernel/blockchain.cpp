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
    std::lock_guard<std::recursive_mutex> lock(chainLock);
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

    return true;
}

CryptoKernel::Blockchain::~Blockchain()
{

}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::getUnconfirmedTransactions()
{
    chainLock.lock();
    const std::set<CryptoKernel::Blockchain::transaction> returning = unconfirmedTransactions;
    chainLock.unlock();

    return returning;
}

CryptoKernel::Blockchain::dbBlock CryptoKernel::Blockchain::getBlockDB(Storage::Transaction* transaction, const std::string& id) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    Json::Value jsonBlock = blocks->get(transaction, id);
    if(!jsonBlock.isObject()) {
        // Check if it's an orphan
        jsonBlock = candidates->get(transaction, id);
        if(!jsonBlock.isObject()) {
            throw NotFoundException("Block " + id);
        } else {
            return dbBlock(block(jsonBlock), 0);
        }
    }

    return dbBlock(jsonBlock);
}

CryptoKernel::Blockchain::dbBlock CryptoKernel::Blockchain::getBlockDB(const std::string& id) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());

    return getBlockDB(tx.get(), id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(Storage::Transaction* transaction, const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    const dbBlock block = getBlockDB(transaction, id);

    return buildBlock(transaction, block);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::buildBlock(Storage::Transaction* dbTx, const dbBlock& dbblock) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::set<transaction> transactions;

    for(const BigNum& txid : dbblock.getTransactions()) {
        transactions.insert(getTransaction(dbTx, txid.toString()));
    }

    return block(transactions, getTransaction(dbTx, dbblock.getCoinbaseTx().toString()), dbblock.getPreviousBlockId(), dbblock.getTimestamp(), dbblock.getConsensusData());
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(Storage::Transaction* transaction, const uint64_t height)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    const std::string id = blocks->get(transaction, std::to_string(height), 0).asString();
    return getBlock(transaction, id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());
    return getBlock(tx.get(), id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(const uint64_t height)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());
    return getBlockByHeight(tx.get(), height);
}

CryptoKernel::Blockchain::output CryptoKernel::Blockchain::getOutput(const std::string& id) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> tx(blockdb->begin());
    return getOutput(tx.get(), id);
}

CryptoKernel::Blockchain::output CryptoKernel::Blockchain::getOutput(Storage::Transaction* dbTx, const std::string& id) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    Json::Value outputJson = utxos->get(dbTx, id);
    if(!outputJson.isObject()) {
        outputJson = stxos->get(dbTx, id);
        if(!outputJson.isObject()) {
            throw NotFoundException("Output " + id);
        }
    }

    return output(outputJson);
}

uint64_t CryptoKernel::Blockchain::getBalance(const std::string& publicKey)
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
        if(utxos->get(dbTransaction, out.getId().toString()).isObject() || stxos->get(dbTransaction, out.getId().toString()).isObject()) {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output already exists");
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

bool CryptoKernel::Blockchain::submitTransaction(const transaction& tx) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    const bool result = submitTransaction(dbTx.get(), tx);
    if(result) {
        dbTx->commit();
    }
    return result;
}

bool CryptoKernel::Blockchain::submitBlock(const block& newBlock, bool genesisBlock) {
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    const bool result = submitBlock(dbTx.get(), newBlock, genesisBlock);
    if(result) {
        dbTx->commit();
    }
    return result;
}

bool CryptoKernel::Blockchain::submitTransaction(Storage::Transaction* dbTx, const transaction& tx)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
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
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Received transaction we already know about");
                return true;
            }
        } else {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to submit transaction to consensus method");
            return false;
        }
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitTransaction(): Failed to verify transaction");
        return false;
    }
}

bool CryptoKernel::Blockchain::submitBlock(Storage::Transaction* dbTx, const block& newBlock, bool genesisBlock)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);

    const std::string idAsString = newBlock.getId().toString();
    //Check block does not already exist
    if(blocks->get(dbTx, idAsString).isObject())
    {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block is already in main chain");
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
            return false;
        }

        if(!consensus->checkConsensusRules(dbTx, newBlock, previousBlock)) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Consensus rules cannot verify this block");
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
                return false;
            }
            fees += calculateTransactionFee(dbTx, tx);
        }

        if(!verifyTransaction(dbTx, newBlock.getCoinbaseTx(), true))
        {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Coinbase transaction could not be verified");
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
            return false;
        }

        if(!consensus->submitBlock(dbTx, newBlock)) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Consensus submitBlock callback returned false");
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

    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): successfully submitted block: " + CryptoKernel::Storage::toString(getBlockDB(dbTx, idAsString).toJson(), true));

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

        inputs->put(dbTransaction, inp.getId().toString(), dbInput(inp).toJson());
    }

    //Add new outputs to UTXOs
    for(const output& out : tx.getOutputs()) {
        utxos->put(dbTransaction, out.getId().toString(), dbOutput(out, tx.getId()).toJson());
    }

    //Commit transaction
    transactions->put(dbTransaction, tx.getId().toString(), Blockchain::dbTransaction(tx, confirmingBlock, coinbaseTx).toJson());

    //Remove transaction from unconfirmed transactions vector
    unconfirmedTransactions.erase(tx);
}

bool CryptoKernel::Blockchain::reorgChain(Storage::Transaction* dbTransaction, const BigNum& newTipId)
{
    std::stack<block> blockList;

    //Find common fork block
    Json::Value blockJson = candidates->get(dbTransaction, newTipId.toString());
    while(blockJson.isObject()) {
        const block currentBlock = block(blockJson);
        blockList.push(currentBlock);
        blockJson = candidates->get(dbTransaction, currentBlock.getPreviousBlockId().toString());
    }

    //Reverse blocks to that point
    const BigNum forkBlockId = blockList.top().getPreviousBlockId();
    while(getBlockDB(dbTransaction, "tip").getId() != forkBlockId) {
        reverseBlock(dbTransaction);
    }

    //Submit new blocks
    while(!blockList.empty()) {
        if(!submitBlock(dbTransaction, blockList.top())) {
            //TODO: should probably blacklist this fork if this happens

            log->printf(LOG_LEVEL_WARN, "blockchain::reorgChain(): New chain failed to verify");

            return false;
        }
        blockList.pop();
    }

    return true;
}

uint64_t CryptoKernel::Blockchain::getTransactionFee(const transaction& tx)
{
    uint64_t fee = 0;

    for(const input& inp : tx.getInputs()) {
        fee += CryptoKernel::Storage::toString(inp.getData()).size() * 100;
    }

    for(const output& out : tx.getOutputs()) {
        fee += CryptoKernel::Storage::toString(out.getData()).size() * 100;
    }

    return fee;
}

uint64_t CryptoKernel::Blockchain::calculateTransactionFee(Storage::Transaction* dbTx, const transaction& tx)
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

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::generateVerifyingBlock(const std::string& publicKey)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());

    const std::set<transaction> blockTransactions = getUnconfirmedTransactions();

    uint64_t height;
    BigNum previousBlockId;
    bool genesisBlock = false;
    try {
        const dbBlock previousBlock = getBlockDB(dbTx.get(), "tip");
        height = previousBlock.getHeight() + 1;
        previousBlockId = previousBlock.getId();
    } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
        height = 1;
        genesisBlock = true;
    }
    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);;

    uint64_t value = getBlockReward(height);

    for(const transaction& tx : blockTransactions)
    {
        value += calculateTransactionFee(dbTx.get(), tx);
    }

    const std::string pubKey = getCoinbaseOwner(publicKey);

    std::default_random_engine generator(now);
    std::uniform_int_distribution<unsigned int> distribution(0, UINT_MAX);
    const uint64_t nonce = distribution(generator);

    Json::Value data;
    data["publicKey"] = pubKey;

    std::set<output> outputs;
    outputs.insert(output(value, nonce, data));

    const transaction coinbaseTx = transaction(std::set<input>(), outputs, now, true);

    Json::Value consensusData;
    if(!genesisBlock) {
        consensusData = consensus->generateConsensusData(dbTx.get(), previousBlockId, publicKey);
    }

    const block returning = block(blockTransactions, coinbaseTx, previousBlockId, now, consensusData);

    return returning;
}

std::set<CryptoKernel::Blockchain::output> CryptoKernel::Blockchain::getUnspentOutputs(const std::string& publicKey)
{
    std::lock_guard<std::recursive_mutex> lock(chainLock);
    std::set<output> returning;

    CryptoKernel::Storage::Table::Iterator* it = new Storage::Table::Iterator(utxos.get(), blockdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        if(it->value()["data"]["publicKey"].asString() == publicKey) {
            returning.insert(output(it->value()));
        }
    }
    delete it;

    return returning;
}

void CryptoKernel::Blockchain::reverseBlock(Storage::Transaction* dbTransaction)
{
    const block tip = getBlock(dbTransaction, "tip");

    for(const output& out : tip.getCoinbaseTx().getOutputs()) {
        utxos->erase(dbTransaction, out.getId().toString());
    }

    transactions->erase(dbTransaction, tip.getCoinbaseTx().getId().toString());

    for(const transaction& tx : tip.getTransactions()) {
        for(const output& out : tx.getOutputs()) {
            utxos->erase(dbTransaction, out.getId().toString());
        }

        for(const input& inp : tx.getInputs()) {
            inputs->erase(dbTransaction, inp.getId().toString());
            const std::string oldOutputId = inp.getOutputId().toString();
            const dbOutput oldOutput = dbOutput(stxos->get(dbTransaction, oldOutputId));
            stxos->erase(dbTransaction, oldOutputId);
            utxos->put(dbTransaction, oldOutputId, oldOutput.toJson());
        }

        transactions->erase(dbTransaction, tx.getId().toString());

        if(!submitTransaction(dbTransaction, tx)) {
            log->printf(LOG_LEVEL_ERR, "Blockchain::reverseBlock(): previously moved transaction is now invalid");
        }
    }

    const dbBlock tipDB = getBlockDB(dbTransaction, "tip");

    blocks->erase(dbTransaction, std::to_string(tipDB.getHeight()), 0);
    blocks->put(dbTransaction, "tip", getBlockDB(dbTransaction, tip.getPreviousBlockId().toString()).toJson());

    candidates->put(dbTransaction, tip.getId().toString(), tip.toJson());
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::getTransaction(Storage::Transaction* transaction, const std::string& id)
{
    const Json::Value jsonTx = transactions->get(transaction, id);
    if(!jsonTx.isObject()) {
        throw NotFoundException("Transaction " + id);
    }

    const dbTransaction tx = dbTransaction(jsonTx);
    std::set<output> outputs;
    for(const BigNum& id : tx.getOutputs()) {
        outputs.insert(getOutput(transaction, id.toString()));
    }

    std::set<input> inps;
    for(const BigNum& id : tx.getInputs()) {
        inps.insert(input(inputs->get(transaction, id.toString())));
    }

    return CryptoKernel::Blockchain::transaction(inps, outputs, tx.getTimestamp(), tx.isCoinbaseTx());
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
