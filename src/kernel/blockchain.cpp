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
#include <thread>

#include "blockchain.h"
#include "crypto.h"
#include "ckmath.h"
#include "contract.h"
#include "schnorr.h"
#include "merkletree.h"

CryptoKernel::Blockchain::Blockchain(CryptoKernel::Log* GlobalLog,
                                     const std::string& dbDir) {
    status = false;
    this->dbDir = dbDir;
    blockdb.reset(new CryptoKernel::Storage(dbDir, false, 20, true));
    blocks.reset(new CryptoKernel::Storage::Table("blocks"));
    transactions.reset(new CryptoKernel::Storage::Table("transactions"));
    utxos.reset(new CryptoKernel::Storage::Table("utxos"));
    stxos.reset(new CryptoKernel::Storage::Table("stxos"));
    inputs.reset(new CryptoKernel::Storage::Table("inputs"));
    candidates.reset(new CryptoKernel::Storage::Table("candidates"));
    log = GlobalLog;
}

bool CryptoKernel::Blockchain::loadChain(CryptoKernel::Consensus* consensus,
                                         const std::string& genesisBlockFile) {
    this->consensus = consensus;
    std::unique_ptr<Storage::Transaction> dbTransaction(blockdb->begin());
    const bool tipExists = blocks->get(dbTransaction.get(), "tip").isObject();
    dbTransaction->abort();
    if(!tipExists) {
        emptyDB();
        bool newGenesisBlock = false;
        std::ifstream t(genesisBlockFile);
        if(!t.is_open()) {
            log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to open genesis block file");
            newGenesisBlock = true;
        } else {
            std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

            block genesisBlock(CryptoKernel::Storage::toJson(buffer));

            if(std::get<0>(submitBlock(genesisBlock, true))) {
                log->printf(LOG_LEVEL_INFO, "blockchain(): Successfully imported genesis block");
            } else {
                log->printf(LOG_LEVEL_WARN, "blockchain(): Failed to import genesis block");
                newGenesisBlock = true;
            }

            t.close();
        }

        if(newGenesisBlock) {
            log->printf(LOG_LEVEL_INFO, "blockchain(): Generating new genesis block");
            CryptoKernel::Crypto crypto(true);
            const block Block = generateVerifyingBlock(crypto.getPublicKey());

            if(!std::get<0>(submitBlock(Block, true))) {
                log->printf(LOG_LEVEL_ERR, "blockchain(): Failed to import new genesis block");
            }

            std::ofstream f;
            f.open(genesisBlockFile);
            f << CryptoKernel::Storage::toString(Block.toJson(), true);
            f.close();
        }
    }

    const block genesisBlock = getBlockByHeight(1);
    genesisBlockId = genesisBlock.getId();

    status = true;

    return true;
}

CryptoKernel::Blockchain::~Blockchain() {

}

std::set<CryptoKernel::Blockchain::transaction>
CryptoKernel::Blockchain::getUnconfirmedTransactions() {
    mempoolMutex.lock();
    const std::set<CryptoKernel::Blockchain::transaction> returning = unconfirmedTransactions.getTransactions();
    mempoolMutex.unlock();

    return returning;
}

CryptoKernel::Blockchain::dbBlock CryptoKernel::Blockchain::getBlockDB(
    Storage::Transaction* transaction, const std::string& id, const bool mainChain) {
    Json::Value jsonBlock = blocks->get(transaction, id);
    if(!jsonBlock.isObject()) {
        // Check if it's an orphan
        jsonBlock = candidates->get(transaction, id);
        if(!jsonBlock.isObject() || mainChain) {
            throw NotFoundException("Block " + id);
        } else {
            return dbBlock(block(jsonBlock));
        }
    }

    return dbBlock(jsonBlock);
}

CryptoKernel::Blockchain::dbBlock CryptoKernel::Blockchain::getBlockDB(
    const std::string& id) {
    std::unique_ptr<Storage::Transaction> tx(blockdb->beginReadOnly());

    return getBlockDB(tx.get(), id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(
    Storage::Transaction* transaction, const std::string& id) {
    const dbBlock block = getBlockDB(transaction, id);

    return buildBlock(transaction, block);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::buildBlock(
    Storage::Transaction* dbTx, const dbBlock& dbblock) {
    std::set<transaction> transactions;

    try {
        for(const BigNum& txid : dbblock.getTransactions()) {
            transactions.insert(getTransaction(dbTx, txid.toString()));
        }

        return block(transactions, getTransaction(dbTx, dbblock.getCoinbaseTx().toString()),
                    dbblock.getPreviousBlockId(), dbblock.getTimestamp(), dbblock.getConsensusData(),
                    dbblock.getHeight());
    } catch(const NotFoundException& e) {
        const Json::Value jsonBlock = candidates->get(dbTx, dbblock.getId().toString());
        if(jsonBlock.isObject()) {
            return block(jsonBlock);
        } else {
            throw;
        }
    }
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(
    Storage::Transaction* transaction, const uint64_t height) {
    const std::string id = blocks->get(transaction, std::to_string(height), 0).asString();
    return getBlock(transaction, id);
}

CryptoKernel::Blockchain::dbBlock CryptoKernel::Blockchain::getBlockByHeightDB(
    Storage::Transaction* transaction, const uint64_t height) {
    const std::string id = blocks->get(transaction, std::to_string(height), 0).asString();
    return getBlockDB(transaction, id);
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::getTransaction(
    const std::string& id) {
    std::unique_ptr<Storage::Transaction> tx(blockdb->beginReadOnly());
    return getTransaction(tx.get(), id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlock(
    const std::string& id) {
    std::unique_ptr<Storage::Transaction> tx(blockdb->beginReadOnly());
    return getBlock(tx.get(), id);
}

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::getBlockByHeight(
    const uint64_t height) {
    std::unique_ptr<Storage::Transaction> tx(blockdb->beginReadOnly());
    return getBlockByHeight(tx.get(), height);
}

CryptoKernel::Blockchain::output CryptoKernel::Blockchain::getOutput(
    const std::string& id) {
    std::unique_ptr<Storage::Transaction> tx(blockdb->beginReadOnly());
    return getOutput(tx.get(), id);
}

CryptoKernel::Blockchain::output CryptoKernel::Blockchain::getOutput(
    Storage::Transaction* dbTx, const std::string& id) {
    Json::Value outputJson = utxos->get(dbTx, id);
    if(!outputJson.isObject()) {
        outputJson = stxos->get(dbTx, id);
        if(!outputJson.isObject()) {
            throw NotFoundException("Output " + id);
        }
    }

    return output(outputJson);
}

CryptoKernel::Blockchain::dbOutput CryptoKernel::Blockchain::getOutputDB(
    Storage::Transaction* dbTx, const std::string& id) {
    Json::Value outputJson = utxos->get(dbTx, id);
    if(!outputJson.isObject()) {
        outputJson = stxos->get(dbTx, id);
        if(!outputJson.isObject()) {
            throw NotFoundException("Output " + id);
        }
    }

    return dbOutput(outputJson);
}

CryptoKernel::Blockchain::input CryptoKernel::Blockchain::getInput(
    Storage::Transaction* dbTx, const std::string& id) {
    Json::Value inputJson = inputs->get(dbTx, id);
    if(!inputJson.isObject()) {
        throw NotFoundException("Input " + id);
    }

    return input(inputJson);
}

std::tuple<bool, bool> CryptoKernel::Blockchain::verifyTransaction(Storage::Transaction* dbTransaction,
        const transaction& tx, const bool coinbaseTx) {
    if(transactions->get(dbTransaction, tx.getId().toString()).isObject()) {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx already exists");
        return std::make_tuple(false, false);
    }

    uint64_t inputTotal = 0;
    uint64_t outputTotal = 0;

    for(const output& out : tx.getOutputs()) {
        if(utxos->get(dbTransaction, out.getId().toString()).isObject() ||
                stxos->get(dbTransaction, out.getId().toString()).isObject()) {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Output already exists");
            //Duplicate output
            return std::make_tuple(false, false);
        }

        outputTotal += out.getValue();
    }

    const CryptoKernel::BigNum outputHash = tx.getOutputSetId();

    std::set<dbOutput> maybeAggregated;

    for(const input& inp : tx.getInputs()) {
        const Json::Value outJson = utxos->get(dbTransaction, inp.getOutputId().toString());
        if(!outJson.isObject()) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::verifyTransaction(): Output has already been spent");
            return std::make_tuple(false, false);
        }

        const dbOutput out = dbOutput(outJson);
        inputTotal += out.getValue();

        const Json::Value outData = out.getData();

        if(!outData["schnorrKey"].empty() && outData["contract"].empty()) {
            const Json::Value spendData = inp.getData();
            if(spendData["signature"].empty() || !spendData["signature"].isString()) {
                maybeAggregated.emplace(out);
            }

            if(!outData["schnorrKey"].isString()) {
                log->printf(LOG_LEVEL_WARN, "blockchain::verifyTransaction(): Output has a malformed schnorr key, not checking its signature");
                maybeAggregated.erase(out);
            } else if(spendData["signature"].isString()) {
                CryptoKernel::Schnorr schnorr;
                if(!schnorr.setPublicKey(outData["schnorrKey"].asString())) {
                    log->printf(LOG_LEVEL_INFO,
                                "blockchain::verifyTransaction(): Schnorr key is malformed");
                    return std::make_tuple(false, true);
                }

                if(!schnorr.verify(out.getId().toString() + outputHash.toString(),
                                spendData["signature"].asString())) {
                    log->printf(LOG_LEVEL_INFO,
                                "blockchain::verifyTransaction(): Could not verify input signature");
                    return std::make_tuple(false, true);
                }
            }
        }

        // Pay-to-merkleroot: Provide the script / pub key and merkle proof + signature
        // If the scripthash/keyhash is contained in the merkle tree, it's considered a
        // valid spend.
        if(!outData["merkleRoot"].empty() && outData["contract"].empty()) {
            const Json::Value spendData = inp.getData();
            
            // Common sense checks
            if(!spendData["spendType"].isString()) {
                log->printf(LOG_LEVEL_INFO,
                    "blockchain::verifyTransaction(): pay-to-merkleroot spendData is malformed (invalid spendType)");
                return std::make_tuple(false, true);
            }

            if(!spendData["pubKeyOrScript"].isString()) {
                log->printf(LOG_LEVEL_INFO,
                    "blockchain::verifyTransaction(): pay-to-merkleroot spendData is malformed (invalid pubKeyOrScript)");
                return std::make_tuple(false, true);
            }

            if(!spendData["merkleProof"].isObject()) {
                log->printf(LOG_LEVEL_INFO,
                    "blockchain::verifyTransaction(): pay-to-merkleroot spendData is malformed (invalid merkleProof)");
                return std::make_tuple(false, true);
            }

            if(spendData["spendType"].asString() == "pubkey" && (spendData["signature"].empty() || !spendData["signature"].isString())) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not verify input signature");
                return std::make_tuple(false, true);
            }

            // Load the merkle proof
            std::shared_ptr<CryptoKernel::MerkleProof> proof;
            try {
                const Json::Value proofJson = spendData["merkleProof"];
                proof = std::make_shared<CryptoKernel::MerkleProof>(proofJson);
            } catch (const CryptoKernel::Blockchain::InvalidElementException ex) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not load merkle proof");
                return std::make_tuple(false, true);
            }

            // Verify if the spending script/pubkey hash is the first item in the proof
            const BigNum& proofValue = proof->leaves.at(0);
            const BigNum& spendValue = CryptoKernel::BigNum(CryptoKernel::Crypto::sha256(spendData["pubKeyOrScript"].asString()));
            if(proofValue != spendValue) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Merkle proof does not start with the spending script or pubkey's hash");
                return std::make_tuple(false, true);             
            }

            // Verify if the proof matches the merkle root
            std::shared_ptr<CryptoKernel::MerkleNode> proofNode = CryptoKernel::MerkleNode::makeMerkleTreeFromProof(proof);
            if(proofNode->getMerkleRoot().toString() != outData["merkleRoot"].asString()) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Merkle proof does not match outData merkle root");

                return std::make_tuple(false, true);             
            }


            if(spendData["spendType"].asString() == "script") {
                CryptoKernel::ContractRunner lvm(this);
                if(!lvm.evaluateScriptValid(dbTransaction, tx, inp, spendData["pubKeyOrScript"].asString())) {
                    log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): P2MAST Script returned false");
                return std::make_tuple(false, true);  
                }
            } else if (spendData["spendType"].asString() == "pubkey") {
                // Verify if the signature is valid for the given pubkey
                // We already checked that that pub key is allowed to spend
                // the input by the checks above.
                CryptoKernel::Crypto crypto;
                crypto.setPublicKey(spendData["pubKeyOrScript"].asString());
                if(!crypto.verify(out.getId().toString() + outputHash.toString(),
                                spendData["signature"].asString())) {
                    log->printf(LOG_LEVEL_INFO,
                                "blockchain::verifyTransaction(): Could not verify input signature for p2mr output");
                    return std::make_tuple(false, true);
                }
                break;
            } else {
                log->printf(LOG_LEVEL_INFO,
                                "blockchain::verifyTransaction(): Invalid spendType for pay-to-merkletree");
                return std::make_tuple(false, true);
            }
        }

        if(!outData["publicKey"].empty() && outData["contract"].empty()) {
            const Json::Value spendData = inp.getData();
            if(spendData["signature"].empty() || !spendData["signature"].isString()) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not verify input signature");
                return std::make_tuple(false, true);
            }

            CryptoKernel::Crypto crypto;
            crypto.setPublicKey(outData["publicKey"].asString());
            if(!crypto.verify(out.getId().toString() + outputHash.toString(),
                              spendData["signature"].asString())) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not verify input signature");
                return std::make_tuple(false, true);
            }
        }
    }

    for(const input& inp : tx.getInputs()) {
        const Json::Value spendData = inp.getData();
        if(spendData["aggregateSignature"].isObject()) {
            if(!spendData["aggregateSignature"]["signs"].isArray() || !spendData["aggregateSignature"]["signature"].isString()) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Aggregate signature malformed. Signs isn't an array or signature isn't a string");
                return std::make_tuple(false, true);
            }

            std::set<uint64_t> signs;
            for(const auto& v : spendData["aggregateSignature"]["signs"]) {
                if(!v.isUInt64()) {
                    log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Aggregate signature malformed. Signs array element isn't an integer");
                    return std::make_tuple(false, true);
                }

                if(v.asUInt64() >= maybeAggregated.size()) {
                    log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Aggregate signature malformed. Signs array value out of range");
                    return std::make_tuple(false, true);
                }

                signs.emplace(v.asUInt64());
            }

            std::set<std::string> pubkeys;
            std::set<BigNum> outputIds;
            for(const auto out : signs) {
                auto it = maybeAggregated.begin();
                std::advance(it, out);

                pubkeys.emplace(it->getData()["schnorrKey"].asString());
                outputIds.emplace(it->getId());
            }

            CryptoKernel::Schnorr schnorr;
            const std::string aggregatedPubkey = schnorr.pubkeyAggregate(pubkeys);
            if(!schnorr.setPublicKey(aggregatedPubkey)) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Aggregate signature malformed. Aggregated pubkey is invalid");
                return std::make_tuple(false, true);
            }

            std::string signaturePayload;
            for(const auto& id : outputIds) {
                signaturePayload += id.toString();
            }
            signaturePayload += outputHash.toString();

            if(!schnorr.verify(signaturePayload, spendData["aggregateSignature"]["signature"].asString())) {
                log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not verify input signature");
                return std::make_tuple(false, true);
            }

            std::set<dbOutput> removals;
            for(const auto out : signs) {
                auto it = maybeAggregated.begin();
                std::advance(it, out);
                removals.emplace(*it);
            }

            for(const auto& out : removals) {
                maybeAggregated.erase(out);
            }
        }
    }

    // There are leftover inputs that weren't covered by an aggregate signature
    if(!maybeAggregated.empty()) {
        log->printf(LOG_LEVEL_INFO,
                            "blockchain::verifyTransaction(): Could not verify input signature. Schnorr output not signed or covered by aggregate signature");
        return std::make_tuple(false, true);
    }

    if(!coinbaseTx) {
        if(outputTotal > inputTotal) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::verifyTransaction(): The output total is greater than the input total");
            return std::make_tuple(false, true);
        }

        uint64_t fee = inputTotal - outputTotal;
        if(fee < getTransactionFee(tx) * 0.5) {
            log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): tx fee is too low");
            return std::make_tuple(false, true);
        }
    }

    CryptoKernel::ContractRunner lvm(this);
    if(!lvm.evaluateValid(dbTransaction, tx)) {
        log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Script returned false");
        return std::make_tuple(false, true);
    }

    if(!consensus->verifyTransaction(dbTransaction, tx)) {
        log->printf(LOG_LEVEL_INFO,
                    "blockchain::verifyTransaction(): Could not verify custom rules");
        return std::make_tuple(false, true);
    }

    log->printf(LOG_LEVEL_INFO, "blockchain::verifyTransaction(): Verified successfully");
       
    return std::make_tuple(true, false);
}

std::tuple<bool, bool> CryptoKernel::Blockchain::submitTransaction(const transaction& tx) {
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    const auto result = submitTransaction(dbTx.get(), tx);
    if(std::get<0>(result)) {
        dbTx->commit();
    }
    return result;
}

std::tuple<bool, bool> CryptoKernel::Blockchain::submitBlock(const block& newBlock, bool genesisBlock) {
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->begin());
    const auto result = submitBlock(dbTx.get(), newBlock, genesisBlock);
    if(std::get<0>(result)) {
        dbTx->commit();
    }
    return result;
}

std::tuple<bool, bool> CryptoKernel::Blockchain::submitTransaction(Storage::Transaction* dbTx,
        const transaction& tx) {
	const auto verifyResult = verifyTransaction(dbTx, tx);
    if(std::get<0>(verifyResult)) {
        if(consensus->submitTransaction(dbTx, tx)) {
            std::lock_guard<std::mutex> lock(mempoolMutex);
			if(unconfirmedTransactions.insert(tx)) {
				log->printf(LOG_LEVEL_INFO,
							"blockchain::submitTransaction(): Received transaction " + tx.getId().toString());
				return std::make_tuple(true, false);
			} else {
				log->printf(LOG_LEVEL_INFO,
							"blockchain::submitTransaction(): " + tx.getId().toString() + " has a mempool conflict");
				return std::make_tuple(false, false);
			}
        } else {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::submitTransaction(): Failed to submit transaction to consensus method");
            return std::make_tuple(false, true);
        }
    } else {
        log->printf(LOG_LEVEL_INFO,
                    "blockchain::submitTransaction(): Failed to verify transaction");
        return verifyResult;
    }
}

std::tuple<bool, bool> CryptoKernel::Blockchain::submitBlock(Storage::Transaction* dbTx,
        const block& Block, bool genesisBlock) {
    block newBlock = Block;

    const std::string idAsString = newBlock.getId().toString();
    //Check block does not already exist
    if(blocks->get(dbTx, idAsString).isObject()) {
        log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Block is already in main chain");
        return std::make_tuple(true, false);
    }

    Json::Value previousBlockJson = blocks->get(dbTx,
                                    newBlock.getPreviousBlockId().toString());
    uint64_t blockHeight = 1;

    bool onlySave = false;

    if(!genesisBlock) {
        if(!previousBlockJson.isObject()) {
            previousBlockJson = candidates->get(dbTx, newBlock.getPreviousBlockId().toString());
            if(!previousBlockJson.isObject()) {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Previous block does not exist");
                return std::make_tuple(false, true);
            }

            const block previousBlock = block(previousBlockJson);
            previousBlockJson = dbBlock(previousBlock).toJson();
        }

        const dbBlock previousBlock = dbBlock(previousBlockJson);

        /*//Check that the timestamp is realistic
        if(newBlock.getTimestamp() < previousBlock.getTimestamp()) {
            log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Timestamp is unrealistic");
            return std::make_tuple(false, true);
        }*/

        if(!consensus->checkConsensusRules(dbTx, newBlock, previousBlock)) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::submitBlock(): Consensus rules cannot verify this block");
            return std::make_tuple(false, true);
        }

        const dbBlock tip = getBlockDB(dbTx, "tip");
        if(previousBlock.getId() != tip.getId()) {
            //This block does not directly lead on from last block
            //Check if the verifier should've come before the current tip
            //If so, reorg, otherwise ignore it
            if(consensus->isBlockBetter(dbTx, newBlock, tip)) {
                log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Forking the chain");
                if(!reorgChain(dbTx, previousBlock.getId())) {
                    log->printf(LOG_LEVEL_INFO, "blockchain::submitBlock(): Alternative chain is not valid");
                    return std::make_tuple(false, true);
                }

                blockHeight = getBlockDB(dbTx, "tip").getHeight() + 1;
            } else {
                log->printf(LOG_LEVEL_WARN,
                            "blockchain::submitBlock(): Chain has less verifier backing than current chain");
                blockHeight = getBlockDB(dbTx, newBlock.getPreviousBlockId().toString()).getHeight() + 1;
                onlySave = true;
            }
        } else {
            blockHeight = tip.getHeight() + 1;
        }
    }

    if(!onlySave) {
        uint64_t fees = 0;

        const unsigned int threads = std::thread::hardware_concurrency();
        const auto& txs = newBlock.getTransactions();
        bool failure = false;
        unsigned int nTx = 0;
        std::vector<std::thread> threadsVec;

        for(const auto& tx : txs) {
            threadsVec.push_back(std::thread([&]{
                if(!std::get<0>(verifyTransaction(dbTx, tx))) {
                    failure = true;
                }
            }));
            nTx++;

            if(nTx % threads == 0 || nTx >= txs.size()) {
                for(auto& thread : threadsVec) {
                    thread.join();
                }
                threadsVec.clear();

                if(failure) {
                    log->printf(LOG_LEVEL_INFO,
                            "blockchain::submitBlock(): Transaction could not be verified");
                    return std::make_tuple(false, true);
                }
            }
        }


        //Verify Transactions
        for(const transaction& tx : newBlock.getTransactions()) {
            fees += calculateTransactionFee(dbTx, tx);
        }

        if(!std::get<0>(verifyTransaction(dbTx, newBlock.getCoinbaseTx(), true))) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::submitBlock(): Coinbase transaction could not be verified");
            return std::make_tuple(false, true);
        }

        uint64_t outputTotal = 0;
        for(const output& out : newBlock.getCoinbaseTx().getOutputs()) {
            outputTotal += out.getValue();
        }

        if(outputTotal > fees + getBlockReward(blockHeight)) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::submitBlock(): Coinbase output is not the correct value");
            return std::make_tuple(false, true);
        }

        if(!consensus->submitBlock(dbTx, newBlock)) {
            log->printf(LOG_LEVEL_INFO,
                        "blockchain::submitBlock(): Consensus submitBlock callback returned false");
            return std::make_tuple(false, true);
        }

        confirmTransaction(dbTx, newBlock.getCoinbaseTx(), newBlock.getId(), true);

        //Move transactions from unconfirmed to confirmed and add transaction utxos to db
        for(const transaction& tx : newBlock.getTransactions()) {
            confirmTransaction(dbTx, tx, newBlock.getId());
        }
    }

    if(onlySave) {
        Json::Value jsonBlock = newBlock.toJson();
        jsonBlock["height"] = blockHeight;
        candidates->put(dbTx, newBlock.getId().toString(), jsonBlock);
    } else {
        const dbBlock toSave = dbBlock(newBlock, blockHeight);
        const Json::Value blockAsJson = toSave.toJson();
        candidates->erase(dbTx, idAsString);
        blocks->put(dbTx, "tip", blockAsJson);
        blocks->put(dbTx, std::to_string(blockHeight), Json::Value(idAsString), 0);
        blocks->put(dbTx, idAsString, blockAsJson);
        std::lock_guard<std::mutex> lock(mempoolMutex);
		unconfirmedTransactions.rescanMempool(dbTx, this);
    }

    if(genesisBlock) {
        genesisBlockId = newBlock.getId();
    }

    log->printf(LOG_LEVEL_INFO,
                "blockchain::submitBlock(): successfully submitted block: " +
                CryptoKernel::Storage::toString(getBlockDB(dbTx, idAsString).toJson(), true));

    return std::make_tuple(true, false);
}

void CryptoKernel::Blockchain::confirmTransaction(Storage::Transaction* dbTransaction,
        const transaction& tx, const BigNum& confirmingBlock, const bool coinbaseTx) {
    //Execute custom transaction rules callback
    if(!consensus->confirmTransaction(dbTransaction, tx)) {
        log->printf(LOG_LEVEL_ERR, "Consensus rules failed to confirm transaction");
    }

    //"Spend" UTXOs
    for(const input& inp : tx.getInputs()) {
        const std::string outputId = inp.getOutputId().toString();
        const Json::Value utxo = utxos->get(dbTransaction, outputId);
        const auto txoData = dbOutput(utxo).getData();

        stxos->put(dbTransaction, outputId, utxo);

        if(!txoData["publicKey"].isNull()) {
            const auto txoStr = txoData["publicKey"].asString() + outputId;

            stxos->put(dbTransaction, txoStr, Json::nullValue, 0);
            utxos->erase(dbTransaction, txoStr, 0);
        }

        utxos->erase(dbTransaction, outputId);

        inputs->put(dbTransaction, inp.getId().toString(), dbInput(inp).toJson());
    }

    //Add new outputs to UTXOs
    for(const output& out : tx.getOutputs()) {
        const auto txoData = out.getData();
        if(!txoData["publicKey"].isNull()) {
            const auto txoStr = txoData["publicKey"].asString() + out.getId().toString();
            utxos->put(dbTransaction, txoStr, Json::nullValue, 0);
        }

        utxos->put(dbTransaction, out.getId().toString(), dbOutput(out, tx.getId()).toJson());
    }

    //Commit transaction
    transactions->put(dbTransaction, tx.getId().toString(), Blockchain::dbTransaction(tx,
                      confirmingBlock, coinbaseTx).toJson());

    //Remove transaction from unconfirmed transactions vector
    std::lock_guard<std::mutex> lock(mempoolMutex);
    unconfirmedTransactions.remove(tx);
}

bool CryptoKernel::Blockchain::reorgChain(Storage::Transaction* dbTransaction,
        const BigNum& newTipId) {
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
        if(!std::get<0>(submitBlock(dbTransaction, blockList.top()))) {
            //TODO: should probably blacklist this fork if this happens

            log->printf(LOG_LEVEL_WARN, "blockchain::reorgChain(): New chain failed to verify");

            return false;
        }
        blockList.pop();
    }

    return true;
}

uint64_t CryptoKernel::Blockchain::getTransactionFee(const transaction& tx) {
    uint64_t fee = 0;

    for(const input& inp : tx.getInputs()) {
        fee += CryptoKernel::Storage::toString(inp.getData()).size() * 100;
    }

    for(const output& out : tx.getOutputs()) {
        fee += CryptoKernel::Storage::toString(out.getData()).size() * 100;
    }

    return fee;
}

uint64_t CryptoKernel::Blockchain::calculateTransactionFee(Storage::Transaction* dbTx,
        const transaction& tx) {
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

CryptoKernel::Blockchain::block CryptoKernel::Blockchain::generateVerifyingBlock(
    const std::string& publicKey) {
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->beginReadOnly());

    std::set<transaction> blockTransactions = getUnconfirmedTransactions();

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

    for(const transaction& tx : blockTransactions) {
        try {
            value += calculateTransactionFee(dbTx.get(), tx);
        } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
            // rare case: the mempool was rescanned and some txs we have are now invalid.
            // For now just return an empty block. In the future the mempool should be in 
            // the database so the mempool and current unspent state are always consistent.
            blockTransactions.clear();
            break;
        }
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

    const block returning = block(blockTransactions, coinbaseTx, previousBlockId, now,
                                  consensusData, height);

    return returning;
}

std::set<CryptoKernel::Blockchain::dbOutput> CryptoKernel::Blockchain::getUnspentOutputs(
    const std::string& publicKey) {
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->beginReadOnly());

    std::set<dbOutput> returning;

    std::unique_ptr<Storage::Table::Iterator> it(new Storage::Table::Iterator(utxos.get(), blockdb.get(), dbTx->snapshot, publicKey, 0));

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        returning.insert(getOutputDB(dbTx.get(), it->key()));
    }

    return returning;
}

std::set<CryptoKernel::Blockchain::dbOutput> CryptoKernel::Blockchain::getSpentOutputs(
    const std::string& publicKey) {
    std::unique_ptr<Storage::Transaction> dbTx(blockdb->beginReadOnly());

    std::set<dbOutput> returning;

    std::unique_ptr<Storage::Table::Iterator> it(new Storage::Table::Iterator(stxos.get(), blockdb.get(), dbTx->snapshot, publicKey, 0));

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        returning.insert(getOutputDB(dbTx.get(), it->key()));
    }

    return returning;
}

void CryptoKernel::Blockchain::reverseBlock(Storage::Transaction* dbTransaction) {
    const block tip = getBlock(dbTransaction, "tip");

    auto eraseUtxo = [&](const auto& out, auto& db) {
        db->erase(dbTransaction, out.getId().toString());

        const auto txoData = out.getData();
        if(!txoData["publicKey"].isNull()) {
            const auto txoStr = txoData["publicKey"].asString() + out.getId().toString();
            db->erase(dbTransaction, txoStr, 0);
        }
    };

    for(const output& out : tip.getCoinbaseTx().getOutputs()) {
        eraseUtxo(out, utxos);
    }

    transactions->erase(dbTransaction, tip.getCoinbaseTx().getId().toString());

	std::set<transaction> replayTxs;

    for(const transaction& tx : tip.getTransactions()) {
        for(const output& out : tx.getOutputs()) {
            eraseUtxo(out, utxos);
        }

        for(const input& inp : tx.getInputs()) {
            inputs->erase(dbTransaction, inp.getId().toString());

            const std::string oldOutputId = inp.getOutputId().toString();
            const dbOutput oldOutput = dbOutput(stxos->get(dbTransaction, oldOutputId));

            eraseUtxo(oldOutput, stxos);

            utxos->put(dbTransaction, oldOutputId, oldOutput.toJson());
            const auto txoData = oldOutput.getData();
            if(!txoData["publicKey"].isNull()) {
                const auto txoStr = txoData["publicKey"].asString() + oldOutputId;
                utxos->put(dbTransaction, txoStr, Json::nullValue, 0);
            }
        }

        transactions->erase(dbTransaction, tx.getId().toString());

		replayTxs.insert(tx);
    }

    const dbBlock tipDB = getBlockDB(dbTransaction, "tip");

    blocks->erase(dbTransaction, std::to_string(tipDB.getHeight()), 0);
    blocks->erase(dbTransaction, tip.getId().toString());
    blocks->put(dbTransaction, "tip", getBlockDB(dbTransaction,
                tip.getPreviousBlockId().toString()).toJson());

    candidates->put(dbTransaction, tip.getId().toString(), tip.toJson());

    mempoolMutex.lock();
	unconfirmedTransactions.rescanMempool(dbTransaction, this);
    mempoolMutex.unlock();

	for(const auto& tx : replayTxs) {
		if(!std::get<0>(submitTransaction(dbTransaction, tx))) {
            log->printf(LOG_LEVEL_WARN,
                        "Blockchain::reverseBlock(): previously moved transaction is now invalid");
        }
	}
}

CryptoKernel::Blockchain::dbTransaction CryptoKernel::Blockchain::getTransactionDB(
    Storage::Transaction* transaction, const std::string& id) {
    const Json::Value jsonTx = transactions->get(transaction, id);
    if(!jsonTx.isObject()) {
        throw NotFoundException("Transaction " + id);
    }

    return dbTransaction(jsonTx);
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::getTransaction(
    Storage::Transaction* transaction, const std::string& id) {
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

    return CryptoKernel::Blockchain::transaction(inps, outputs, tx.getTimestamp(),
            tx.isCoinbaseTx());
}

void CryptoKernel::Blockchain::emptyDB() {
    blockdb.reset();
    CryptoKernel::Storage::destroy(dbDir);
    blockdb.reset(new CryptoKernel::Storage(dbDir, false, 20, true));
}

CryptoKernel::Storage::Transaction* CryptoKernel::Blockchain::getTxHandle() {
    Storage::Transaction* dbTx = blockdb->beginReadOnly();
    return dbTx;
}

CryptoKernel::Blockchain::Mempool::Mempool() {
	bytes = 0;
}

bool CryptoKernel::Blockchain::Mempool::insert(const transaction& tx) {
	// Check if any inputs or outputs conflict
	if(txs.find(tx.getId()) != txs.end()) {
		return false;
	}

	for(const input& inp : tx.getInputs()) {
		if(inputs.find(inp.getId()) != inputs.end()) {
			return false;
		}

        if(outputs.find(inp.getOutputId()) != outputs.end()) {
            return false;
        }
	}

	for(const output& out : tx.getOutputs()) {
		if(outputs.find(out.getId()) != outputs.end()) {
			return false;
		}
	}

	txs.insert(std::pair<BigNum, transaction>(tx.getId(), tx));

    bytes += tx.size();

	for(const input& inp : tx.getInputs()) {
		inputs.insert(std::pair<BigNum, BigNum>(inp.getId(), tx.getId()));
        outputs.insert(std::pair<BigNum, BigNum>(inp.getOutputId(), tx.getId()));
	}

	for(const output& out : tx.getOutputs()) {
		outputs.insert(std::pair<BigNum, BigNum>(out.getId(), tx.getId()));
	}

	return true;
}

void CryptoKernel::Blockchain::Mempool::remove(const transaction& tx) {
	if(txs.find(tx.getId()) != txs.end()) {
		txs.erase(tx.getId());

        bytes -= tx.size();

		for(const input& inp : tx.getInputs()) {
			inputs.erase(inp.getId());
            outputs.erase(inp.getOutputId());
		}

		for(const output& out : tx.getOutputs()) {
			outputs.erase(out.getId());
		}
	}
}

void CryptoKernel::Blockchain::Mempool::rescanMempool(Storage::Transaction* dbTx, Blockchain* blockchain) {
	std::set<transaction> removals;

	for(const auto& tx : txs) {
        if(!std::get<0>(blockchain->verifyTransaction(dbTx, tx.second))) {
			removals.insert(tx.second);
		}
	}

	for(const auto& tx : removals) {
		remove(tx);
	}
}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::Mempool::getTransactions() const {
	uint64_t totalSize = 0;
	std::set<transaction> returning;

	for(const auto& it : txs) {
		if(totalSize + it.second.size() < 3.9 * 1024 * 1024) {
			returning.insert(it.second);
			totalSize += it.second.size();
			continue;
		}

		break;
	}

	return returning;
}

unsigned int CryptoKernel::Blockchain::Mempool::count() const {
    return txs.size();
}

unsigned int CryptoKernel::Blockchain::Mempool::size() const {
    return bytes;
}

unsigned int CryptoKernel::Blockchain::mempoolCount() const {
    return unconfirmedTransactions.count();
}

unsigned int CryptoKernel::Blockchain::mempoolSize() const {
    return unconfirmedTransactions.size();
}
