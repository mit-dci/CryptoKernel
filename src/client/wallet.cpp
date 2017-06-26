/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2017  James Lovejoy

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

#include <sstream>

#include "wallet.h"
#include "crypto.h"

CryptoKernel::Wallet::Wallet(CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network, CryptoKernel::Log* log) {
    this->blockchain = blockchain;
    this->network = network;
    this->log = log;

    walletdb.reset(new CryptoKernel::Storage("./addressesdb"));
    accounts.reset(new CryptoKernel::Storage::Table("accounts"));
    utxos.reset(new CryptoKernel::Storage::Table("utxos"));
    transactions.reset(new CryptoKernel::Storage::Table("transactions"));
    params.reset(new CryptoKernel::Storage::Table("params"));

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
    const Json::Value height = params->get(dbTx.get(), "height");
    if(height.isNull()) {
        params->put(dbTx.get(), "height", Json::Value(0));
        params->put(dbTx.get(), "tipId", Json::Value(""));
        dbTx->commit();
    }

    const time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));

    running = true;
    watchThread.reset(new std::thread(&CryptoKernel::Wallet::watchFunc, this));
}

CryptoKernel::Wallet::~Wallet() {
    running = false;
    watchThread->join();
}

void CryptoKernel::Wallet::watchFunc() {
    while(running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        /*
            Steps:
                - Compare sync height and block hash to check for forks
                - If tip is higher, sync up to tip
                - Otherwise, if there is a mismatch, rewind to fork block
        */
        std::lock_guard<std::recursive_mutex> lock(walletLock);
        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
        std::unique_ptr<CryptoKernel::Storage::Transaction> bchainTx(blockchain->getTxHandle());

        bool rewind = false;
        do {
            rewind = false;
            const uint64_t height = params->get(dbTx.get(), "height").asUInt64();
            const std::string tipId = params->get(dbTx.get(), "tipId").asString();
            if(height > 0) {
                try {
                    const CryptoKernel::Blockchain::dbBlock syncBlock = blockchain->getBlockByHeightDB(bchainTx.get(), height);
                    if(syncBlock.getId().toString() != tipId) {
                        // There was a fork, rewind to fork block
                        rewind = true;
                    }
                } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                    rewind = true;
                }

                if(rewind) {
                    try {
                        rewindBlock(dbTx.get(), bchainTx.get());
                    } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                        dbTx->abort();
                        clearDB();
                        dbTx.reset(walletdb->begin());
                    }
                }
            }
        } while(rewind);

        // Forks resolved, sync to current tip
        const CryptoKernel::Blockchain::dbBlock tipBlock = blockchain->getBlockDB(bchainTx.get(), "tip");
        uint64_t height = params->get(dbTx.get(), "height").asUInt64();
        while(height < tipBlock.getHeight()) {
            const CryptoKernel::Blockchain::block currentBlock = blockchain->getBlockByHeight(bchainTx.get(), height + 1);
            digestBlock(dbTx.get(), bchainTx.get(), currentBlock);
            height = params->get(dbTx.get(), "height").asUInt64();
        }

        bchainTx->abort();
        dbTx->commit();
    }
}

void CryptoKernel::Wallet::rewindBlock(CryptoKernel::Storage::Transaction* walletTx, CryptoKernel::Storage::Transaction* bchainTx) {
    /*
        Steps
            - Get current block tip from wallet perspective
            - Delete new outputs and restore old ones
            - Delete new transactions
    */
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    const std::string tipId = params->get(walletTx, "tipId").asString();
    const CryptoKernel::Blockchain::block oldTip = blockchain->getBlock(bchainTx, tipId);

    log->printf(LOG_LEVEL_INFO, "Wallet::rewindBlock(): Rewinding block " + std::to_string(oldTip.getHeight()));

    for(const CryptoKernel::Blockchain::transaction& tx : oldTip.getTransactions()) {
        const Json::Value txJson = transactions->get(walletTx, tx.getId().toString());
        if(!txJson.isNull()) {
            transactions->erase(walletTx, tx.getId().toString());

            for(const CryptoKernel::Blockchain::output& out : tx.getOutputs()) {
                const Json::Value outJson = utxos->get(walletTx, out.getId().toString());
                if(outJson.isObject()) {
                    utxos->erase(walletTx, out.getId().toString());

                    Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                    acc.setBalance(acc.getBalance() - out.getValue());
                    accounts->put(walletTx, acc.getName(), acc.toJson());
                }
            }

            for(const CryptoKernel::Blockchain::input& inp : tx.getInputs()) {
                const CryptoKernel::Blockchain::output out = blockchain->getOutput(bchainTx, inp.getOutputId().toString());
                if(out.getData()["publicKey"].isString()) {
                    try {
                        Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                        acc.setBalance(acc.getBalance() + out.getValue());
                        accounts->put(walletTx, acc.getName(), acc.toJson());
                    } catch(const WalletException& e) {
                        continue;
                    }
                }
                const Txo newTxo = Txo(out.getId().toString(), out.getValue());
                utxos->put(walletTx, out.getId().toString(), newTxo.toJson());
            }
        }
    }

    const CryptoKernel::Blockchain::dbBlock newTip = blockchain->getBlockDB(bchainTx, oldTip.getPreviousBlockId().toString());

    params->put(walletTx, "tipId", Json::Value(newTip.getId().toString()));
    params->put(walletTx, "height", Json::Value(static_cast<unsigned long long int>(newTip.getHeight())));
}

void CryptoKernel::Wallet::clearDB() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    std::set<std::string> transactionIds;
    std::set<std::string> utxoIds;
    std::set<std::string> accountNames;

    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(transactions.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        transactionIds.insert(it->key());
    }
    delete it;

    it = new CryptoKernel::Storage::Table::Iterator(utxos.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        utxoIds.insert(it->key());
    }
    delete it;

    it = new CryptoKernel::Storage::Table::Iterator(accounts.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        accountNames.insert(it->key());
    }
    delete it;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    for(const auto& tx : transactionIds) {
        transactions->erase(dbTx.get(), tx);
    }

    for(const auto& utxo : utxoIds) {
        utxos->erase(dbTx.get(), utxo);
    }

    for(const auto& acc : accountNames) {
        Account account = Account(accounts->get(dbTx.get(), acc));
        account.setBalance(0);
        accounts->put(dbTx.get(), acc, account.toJson());
    }

    params->put(dbTx.get(), "height", Json::Value(0));
    params->put(dbTx.get(), "tipId", Json::Value(""));

    dbTx->commit();
}

void CryptoKernel::Wallet::digestBlock(CryptoKernel::Storage::Transaction* walletTx, CryptoKernel::Storage::Transaction* bchainTx, const CryptoKernel::Blockchain::block& block) {
    /*
        Steps
            - Scan block for spent outputs that belong to us
            - Import new inputs that belong to us
    */
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    log->printf(LOG_LEVEL_INFO, "Wallet::digestBlock(): Digesting block " + std::to_string(block.getHeight()));

    for(const CryptoKernel::Blockchain::transaction& tx : block.getTransactions()) {
        bool trackTx = false;

        for(const CryptoKernel::Blockchain::input& inp : tx.getInputs()) {
            const Json::Value txo = utxos->get(walletTx, inp.getOutputId().toString());
            if(txo.isObject()) {
                trackTx = true;
                utxos->erase(walletTx, inp.getOutputId().toString());

                const CryptoKernel::Blockchain::output out = blockchain->getOutput(bchainTx, inp.getOutputId().toString());
                Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                acc.setBalance(acc.getBalance() - out.getValue());
                accounts->put(walletTx, acc.getName(), acc.toJson());
            }
        }

        for(const CryptoKernel::Blockchain::output& out : tx.getOutputs()) {
            // Check if there is a publicKey that belongs to you
            if(out.getData()["publicKey"].isString()) {
                try {
                    Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                    acc.setBalance(acc.getBalance() + out.getValue());
                    accounts->put(walletTx, acc.getName(), acc.toJson());
                } catch(const WalletException& e) {
                    continue;
                }
            }
            trackTx = true;
            const Txo newTxo = Txo(out.getId().toString(), out.getValue());
            utxos->put(walletTx, out.getId().toString(), newTxo.toJson());
        }

        if(trackTx) {
            transactions->put(walletTx, tx.getId().toString(), Json::Value(true));
        }
    }

    params->put(walletTx, "height", Json::Value(static_cast<unsigned long long int>(block.getHeight())));
    params->put(walletTx, "tipId", Json::Value(block.getId().toString()));
}

CryptoKernel::Wallet::Account::Account(const std::string& name) {
    this->name = name;
    balance = 0;

    const keyPair newKey = newAddress();
    keys.insert(newKey);
}

Json::Value CryptoKernel::Wallet::Account::toJson() const {
    Json::Value returning;

    returning["name"] = name;
    returning["balance"] = static_cast<unsigned long long int>(balance);

    for(const keyPair& key : keys) {
        Json::Value jsonKeyPair;
        jsonKeyPair["pubKey"] = key.pubKey;
        jsonKeyPair["privKey"] = key.privKey;
        returning["keys"].append(jsonKeyPair);
    }

    return returning;
}

CryptoKernel::Wallet::Account::Account(const Json::Value& accountJson) {
    name = accountJson["name"].asString();
    balance = accountJson["balance"].asUInt64();

    for(const Json::Value& key : accountJson["keys"]) {
        keyPair newKeys;
        newKeys.pubKey = key["pubKey"].asString();
        newKeys.privKey = key["privKey"].asString();
        keys.insert(newKeys);
    }
}

uint64_t CryptoKernel::Wallet::Account::getBalance() const {
    return balance;
}

void CryptoKernel::Wallet::Account::setBalance(const uint64_t newBalance) {
    balance = newBalance;
}

std::string CryptoKernel::Wallet::Account::getName() const {
    return name;
}

std::set<CryptoKernel::Wallet::Account::keyPair> CryptoKernel::Wallet::Account::getKeys() const {
    return keys;
}

CryptoKernel::Wallet::Account::keyPair CryptoKernel::Wallet::Account::newAddress() {
    CryptoKernel::Crypto crypto(true);

    keyPair newKey;
    newKey.pubKey = crypto.getPublicKey();
    newKey.privKey = crypto.getPrivateKey();

    keys.insert(newKey);

    return newKey;
}

bool CryptoKernel::Wallet::Account::operator<(const CryptoKernel::Wallet::Account& rhs) const {
    return getName() < rhs.getName();
}

CryptoKernel::Wallet::Txo::Txo(const std::string& id, const uint64_t value) {
    this->id = id;
    this->spent = false;
    this->value = value;
}

CryptoKernel::Wallet::Txo::Txo(const Json::Value& txoJson) {
    id = txoJson["id"].asString();
    spent = txoJson["spent"].asBool();
    value = txoJson["value"].asUInt64();
}

Json::Value CryptoKernel::Wallet::Txo::toJson() const {
    Json::Value returning;

    returning["id"] = id;
    returning["spent"] = spent;
    returning["value"] = static_cast<unsigned long long int>(value);

    return returning;
}

void CryptoKernel::Wallet::Txo::spend() {
    spent = true;
}

bool CryptoKernel::Wallet::Txo::isSpent() const {
    return spent;
}

uint64_t CryptoKernel::Wallet::Txo::getValue() const {
    return value;
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByName(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    const Json::Value accountJson = accounts->get(dbTx.get(), name);
    if(accountJson.isObject()) {
        return Account(accountJson);
    } else {
        throw WalletException("Account not found for given name");
    }
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByKey(const std::string& pubKey) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    return getAccountByKey(dbTx.get(), pubKey);
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByKey(CryptoKernel::Storage::Transaction* dbTx, const std::string& pubKey) {
    const Json::Value accName = accounts->get(dbTx, pubKey, 0);
    if(!accName.isNull()) {
        const Account acc = Account(accounts->get(dbTx, accName.asString()));
        return acc;
    }

    throw WalletException("Account not found for given pubKey");
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::newAccount(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    try {
        getAccountByName(name);
    } catch(const WalletException& e) {
        const Account acc = Account(name);
        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
        accounts->put(dbTx.get(), name, acc.toJson());
        accounts->put(dbTx.get(), (*acc.getKeys().begin()).pubKey, name, 0);
        dbTx->commit();
        return acc;
    }

    throw WalletException("Account already exists");
}

std::string CryptoKernel::Wallet::sendToAddress(const std::string& pubKey, const uint64_t amount) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> bchainTx(blockchain->getTxHandle());

    if(getTotalBalance() * 100000000 < amount) {
        return "Insufficient funds";
    }

    CryptoKernel::Crypto crypto;
    if(!crypto.setPublicKey(pubKey)) {
        return "Invalid address";
    }

    std::set<CryptoKernel::Blockchain::output> toSpend;
    uint64_t accumulator = 0;
    uint64_t fee = 15000;

    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(utxos.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const Txo out = Txo(it->value());
        if(accumulator < amount + fee) {
            if(!out.isSpent()) {
                const CryptoKernel::Blockchain::output fullOut = blockchain->getOutput(bchainTx.get(), it->key());
                if(fullOut.getData()["contract"].isNull()) {
                    fee += CryptoKernel::Storage::toString(fullOut.getData()).size() * 60;
                    toSpend.insert(fullOut);
                    accumulator += fullOut.getValue();
                }
            }
        } else {
            break;
        }
    }
    delete it;

    if(accumulator < amount + fee) {
        return "Insufficient funds when " + std::to_string(fee / 100000000.0) + " fee is included";
    }

    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());
    Json::Value data;
    data["publicKey"] = pubKey;

    const CryptoKernel::Blockchain::output toThem = CryptoKernel::Blockchain::output(amount, distribution(generator), data);

    std::stringstream buffer;
    buffer << distribution(generator) << "_change";
    const Account account = newAccount(buffer.str());

    data.clear();
    data["publicKey"] = (*(account.getKeys().begin())).pubKey;

    const CryptoKernel::Blockchain::output change = CryptoKernel::Blockchain::output(accumulator - amount - fee, distribution(generator), data);

    std::set<CryptoKernel::Blockchain::output> outputs;
    outputs.insert(change);
    outputs.insert(toThem);

    const std::string outputHash = CryptoKernel::Blockchain::transaction::getOutputSetId(outputs).toString();

    std::set<CryptoKernel::Blockchain::input> spends;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    for(const CryptoKernel::Blockchain::output& out : toSpend) {
        const std::string publicKey = out.getData()["publicKey"].asString();
        Account acc = getAccountByKey(dbTx.get(), publicKey);

        std::string privKey = "";

        for(const auto& key : acc.getKeys()) {
            if(key.pubKey == publicKey) {
                privKey = key.privKey;
                break;
            }
        }

        crypto.setPrivateKey(privKey);

        const std::string signature = crypto.sign(out.getId().toString() + outputHash);
        Json::Value spendData;
        spendData["signature"] = signature;

        const CryptoKernel::Blockchain::input inp = CryptoKernel::Blockchain::input(out.getId(), spendData);
        spends.insert(inp);

        Txo utxo = Txo(utxos->get(dbTx.get(), out.getId().toString()));
        utxo.spend();
        utxos->put(dbTx.get(), out.getId().toString(), utxo.toJson());
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::transaction(spends, outputs, now);

    bchainTx->abort();

    if(!blockchain->submitTransaction(tx)) {
        return "Error submitting transaction";
    }

    dbTx->commit();

    std::vector<CryptoKernel::Blockchain::transaction> txs;
    txs.push_back(tx);
    network->broadcastTransactions(txs);

    return tx.getId().toString();
}

uint64_t CryptoKernel::Wallet::getTotalBalance() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new CryptoKernel::Storage::Table::Iterator(utxos.get(), walletdb.get()));

    uint64_t total = 0;

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const Txo utxo = Txo(it->value());
        if(!utxo.isSpent()) {
            total += utxo.getValue();
        }
    }

    return total;
}

std::set<CryptoKernel::Wallet::Account> CryptoKernel::Wallet::listAccounts() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new CryptoKernel::Storage::Table::Iterator(accounts.get(), walletdb.get()));

    std::set<CryptoKernel::Wallet::Account> returning;

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const Account account = Account(it->value());
        returning.insert(account);
    }

    return returning;
}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Wallet::listTransactions() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new CryptoKernel::Storage::Table::Iterator(transactions.get(), walletdb.get()));

    std::set<CryptoKernel::Blockchain::transaction> returning;

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const CryptoKernel::Blockchain::transaction tx = blockchain->getTransaction(it->key());
        returning.insert(tx);
    }

    return returning;
}

CryptoKernel::Blockchain::transaction CryptoKernel::Wallet::signTransaction(const CryptoKernel::Blockchain::transaction& tx) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    const std::string outputHash = tx.getOutputSetId().toString();

    CryptoKernel::Crypto crypto;

    std::set<CryptoKernel::Blockchain::input> newInputs;

    for(const CryptoKernel::Blockchain::input& input : tx.getInputs()) {
        // Look up UTXO to get publicKey
        Json::Value outputData;
        try {
            outputData = blockchain->getOutput(input.getOutputId().toString()).getData();
        } catch(CryptoKernel::Blockchain::NotFoundException e) {
            // TODO: throw an error here rather than fail silently
            return tx;
        }

        const Account acc = getAccountByKey(outputData["publicKey"].asString());

        std::string privKey = "";

        for(const auto& key : acc.getKeys()) {
            if(key.pubKey == outputData["publicKey"].asString()) {
                privKey = key.privKey;
                break;
            }
        }

        crypto.setPrivateKey(privKey);
        const std::string signature = crypto.sign(input.getOutputId().toString() + outputHash);
        Json::Value spendData = input.getData();
        spendData["signature"] = signature;
        newInputs.insert(CryptoKernel::Blockchain::input(input.getOutputId(), spendData));
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    return CryptoKernel::Blockchain::transaction(newInputs, tx.getOutputs(), now);
}
