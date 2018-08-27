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
#include <iostream>

#include "wallet.h"
#include "crypto.h"

CryptoKernel::Wallet::Wallet(CryptoKernel::Blockchain* blockchain,
                             CryptoKernel::Network* network,
                             CryptoKernel::Log* log,
                             const std::string& dbDir) {
    this->blockchain = blockchain;
    this->network = network;
    this->log = log;

    walletdb.reset(new CryptoKernel::Storage(dbDir, true, 8, false));
    accounts.reset(new CryptoKernel::Storage::Table("accounts"));
    utxos.reset(new CryptoKernel::Storage::Table("utxos"));
    transactions.reset(new CryptoKernel::Storage::Table("transactions"));
    params.reset(new CryptoKernel::Storage::Table("params"));

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
    const Json::Value height = params->get(dbTx.get(), "height");
    const Json::Value schemaV = params->get(dbTx.get(), "schemaVersion");
    schemaVersion = schemaV.asUInt64();
    if(height.isNull()) {
        params->put(dbTx.get(), "height", Json::Value(0));
        params->put(dbTx.get(), "tipId", Json::Value(""));
        params->put(dbTx.get(), "schemaVersion", Json::Value(LATEST_WALLET_SCHEMA));
        dbTx->commit();
        upgradeWallet();
    } else if(schemaVersion < LATEST_WALLET_SCHEMA) {
        // Upgrade required
        dbTx->abort();
        upgradeWallet();
    } else if(schemaVersion > LATEST_WALLET_SCHEMA) {
        throw std::runtime_error("Wallet schema version is newer than this software");
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

void CryptoKernel::Wallet::upgradeWallet() {
    log->printf(LOG_LEVEL_INFO, "Wallet(): Wallet upgrade started " + std::to_string(schemaVersion)
                                + " -> " + std::to_string(LATEST_WALLET_SCHEMA));
    if(schemaVersion == 0) {
        // Ask for wallet passphrase
        std::string password;
        std::string passwordConfirm;

        while(password != passwordConfirm || password.size() < 8) {
            password.clear();
            passwordConfirm.clear();

            password = getPass("Enter new wallet passphrase (min 8 chars): ");

            passwordConfirm = getPass("Confirm passphrase: ");
        }

        std::vector<Json::Value> accountsJson;

        // Encrypt private keys
        auto it = new CryptoKernel::Storage::Table::Iterator(accounts.get(), walletdb.get());
        for(it->SeekToFirst(); it->Valid(); it->Next()) {
            accountsJson.push_back(it->value());
        }
        delete it;

        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

        AES256 pwCheck(password, "CORRECT");
        params->put(dbTx.get(), "pwcheck", pwCheck.toJson());

        for(auto& acc : accountsJson) {
            for(auto& key : acc["keys"]) {
                AES256 crypt(password, key["privKey"].asString());
                key["privKey"] = crypt.toJson();
            }

            accounts->put(dbTx.get(), acc["name"].asString(), acc);
        }

        params->put(dbTx.get(), "schemaVersion", Json::Value(1));

        dbTx->commit();

        std::cout << "Wallet successfully encrypted" << std::endl;

        try {
            newAccount("mining", password);
        } catch(const WalletException& e) {}

        schemaVersion = 1;
    }

    if(schemaVersion == 1) {
        std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new
            CryptoKernel::Storage::Table::Iterator(transactions.get(), walletdb.get()));

        std::set<std::string> txIds;

        for(it->SeekToFirst(); it->Valid(); it->Next()) {
            txIds.insert(it->key());
        }

        it.reset();

        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

        for(const auto& txId : txIds) {
            Json::Value tx;
            tx["unconfirmed"] = false;

            transactions->put(dbTx.get(), txId, tx);
        }

        params->put(dbTx.get(), "schemaVersion", Json::Value(2));

        dbTx->commit();

        schemaVersion = 2;
    }

    schemaVersion = LATEST_WALLET_SCHEMA;

    log->printf(LOG_LEVEL_INFO, "Wallet(): Wallet upgrade complete");
}

bool CryptoKernel::Wallet::checkPassword(const std::string& password) {
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    AES256 pwCheck(params->get(dbTx.get(), "pwcheck"));
    try {
        if(pwCheck.decrypt(password) == "CORRECT") {
            return true;
        }
    } catch(const std::runtime_error& e) {
        return false;
    }

    return false;
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
                    const CryptoKernel::Blockchain::dbBlock syncBlock = blockchain->getBlockByHeightDB(
                                bchainTx.get(), height);
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
        const CryptoKernel::Blockchain::dbBlock tipBlock = blockchain->getBlockDB(bchainTx.get(),
                "tip");
        uint64_t height = params->get(dbTx.get(), "height").asUInt64();
        while(height < tipBlock.getHeight()) {
            const CryptoKernel::Blockchain::block currentBlock = blockchain->getBlockByHeight(
                        bchainTx.get(), height + 1);
            digestBlock(dbTx.get(), bchainTx.get(), currentBlock);
            height = params->get(dbTx.get(), "height").asUInt64();
        }

        // get the unconfirmed transactions
        const std::set<CryptoKernel::Blockchain::transaction> unconfirmedTxs = blockchain->getUnconfirmedTransactions();

        // check to see if they are relevant
        for(const CryptoKernel::Blockchain::transaction& tx : unconfirmedTxs) {
            digestTx(tx, dbTx.get(), bchainTx.get(), true);
        }

        bchainTx->abort();
        dbTx->commit();
    }
}

void CryptoKernel::Wallet::rewindTx(const CryptoKernel::Blockchain::transaction& tx,
                     CryptoKernel::Storage::Transaction* walletTx,
                     CryptoKernel::Storage::Transaction* bchainTx) {
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
            const CryptoKernel::Blockchain::output out = blockchain->getOutputDB(bchainTx,
                    inp.getOutputId().toString());
            if(out.getData()["publicKey"].isString()) {
                try {
                    Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                    acc.setBalance(acc.getBalance() + out.getValue());
                    accounts->put(walletTx, acc.getName(), acc.toJson());
                } catch(const WalletException& e) {
                    continue;
                }

                const Txo newTxo = Txo(out.getId().toString(), out.getValue());
                utxos->put(walletTx, out.getId().toString(), newTxo.toJson());
            }
        }
    }
}


void CryptoKernel::Wallet::rewindBlock(CryptoKernel::Storage::Transaction* walletTx,
                                       CryptoKernel::Storage::Transaction* bchainTx) {
    /*
        Steps
            - Get current block tip from wallet perspective
            - Delete new outputs and restore old ones
            - Delete new transactions
    */
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    const std::string tipId = params->get(walletTx, "tipId").asString();
    const CryptoKernel::Blockchain::block oldTip = blockchain->buildBlock(bchainTx, blockchain->getBlockDB(bchainTx, tipId));

    log->printf(LOG_LEVEL_INFO,
                "Wallet::rewindBlock(): Rewinding block " + std::to_string(oldTip.getHeight()));

    std::set<CryptoKernel::Blockchain::transaction> txs = oldTip.getTransactions();
    txs.insert(oldTip.getCoinbaseTx());

    for(const CryptoKernel::Blockchain::transaction& tx : txs) {
        rewindTx(tx, walletTx, bchainTx);
    }

    const CryptoKernel::Blockchain::dbBlock newTip = blockchain->getBlockDB(bchainTx,
            oldTip.getPreviousBlockId().toString());

    params->put(walletTx, "tipId", Json::Value(newTip.getId().toString()));
    params->put(walletTx, "height",
                Json::Value(newTip.getHeight()));
}

void CryptoKernel::Wallet::clearDB() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    std::set<std::string> transactionIds;
    std::set<std::string> utxoIds;
    std::set<std::string> accountNames;

    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(
        transactions.get(), walletdb.get());
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


void CryptoKernel::Wallet::digestTx(const CryptoKernel::Blockchain::transaction& tx,
                 CryptoKernel::Storage::Transaction* walletTx,
                 CryptoKernel::Storage::Transaction* bchainTx,
                 const bool unconfirmed) {
    bool trackTx = false;

    for(const CryptoKernel::Blockchain::input& inp : tx.getInputs()) {
        const Json::Value txo = utxos->get(walletTx, inp.getOutputId().toString());
        if(txo.isObject()) {
            trackTx = true;
            if(!unconfirmed) {
                utxos->erase(walletTx, inp.getOutputId().toString());

                const CryptoKernel::Blockchain::output out = blockchain->getOutput(bchainTx,
                        inp.getOutputId().toString());
                Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                acc.setBalance(acc.getBalance() - out.getValue());
                accounts->put(walletTx, acc.getName(), acc.toJson());
            }
        }
    }

    for(const CryptoKernel::Blockchain::output& out : tx.getOutputs()) {
        // Check if there is a publicKey that belongs to you
        if(out.getData()["publicKey"].isString()) {
            try {
                Account acc = getAccountByKey(walletTx, out.getData()["publicKey"].asString());
                if(!unconfirmed) {
                    acc.setBalance(acc.getBalance() + out.getValue());
                    accounts->put(walletTx, acc.getName(), acc.toJson());
                }
            } catch(const WalletException& e) {
                continue;
            }

            trackTx = true;
        
            if(!unconfirmed) {
                const Txo newTxo = Txo(out.getId().toString(), out.getValue());
                utxos->put(walletTx, out.getId().toString(), newTxo.toJson());
            }
        }
    }

    if(trackTx) {
        Json::Value txJson;
        txJson["unconfirmed"] = unconfirmed;
        transactions->put(walletTx, tx.getId().toString(), txJson);
    }
}

void CryptoKernel::Wallet::digestBlock(CryptoKernel::Storage::Transaction* walletTx,
                                       CryptoKernel::Storage::Transaction* bchainTx,
                                       const CryptoKernel::Blockchain::block& block) {
    /*
        Steps
            - Scan block for spent outputs that belong to us
            - Import new inputs that belong to us
    */
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    log->printf(LOG_LEVEL_INFO,
                "Wallet::digestBlock(): Digesting block " + std::to_string(block.getHeight()));

    std::set<CryptoKernel::Blockchain::transaction> txs = block.getTransactions();
    txs.insert(block.getCoinbaseTx());

    for(const CryptoKernel::Blockchain::transaction& tx : txs) {
        digestTx(tx, walletTx, bchainTx);
    }

    params->put(walletTx, "height",
                Json::Value(block.getHeight()));
    params->put(walletTx, "tipId", Json::Value(block.getId().toString()));
}

CryptoKernel::Wallet::Account::Account(const std::string& name, const std::string& password) {
    this->name = name;
    balance = 0;

    const keyPair newKey = newAddress(password);
    keys.insert(newKey);
}

Json::Value CryptoKernel::Wallet::Account::toJson() const {
    Json::Value returning;

    returning["name"] = name;
    returning["balance"] = balance;

    for(const keyPair& key : keys) {
        Json::Value jsonKeyPair;
        jsonKeyPair["pubKey"] = key.pubKey;
        jsonKeyPair["privKey"] = key.privKey->toJson();
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
        newKeys.privKey.reset(new AES256(key["privKey"]));
        keys.insert(newKeys);
    }
}

uint64_t CryptoKernel::Wallet::Account::getBalance() const {
    return balance;
}

void CryptoKernel::Wallet::Account::setBalance(const uint64_t newBalance) {
    balance = newBalance;
}

void CryptoKernel::Wallet::Account::addKeyPair(const keyPair& kp) {
    keys.insert(kp);
}

std::string CryptoKernel::Wallet::Account::getName() const {
    return name;
}

std::set<CryptoKernel::Wallet::Account::keyPair> CryptoKernel::Wallet::Account::getKeys()
const {
    return keys;
}

CryptoKernel::Wallet::Account::keyPair
CryptoKernel::Wallet::Account::newAddress(const std::string& password) {
    CryptoKernel::Crypto crypto(true);

    keyPair newKey;
    newKey.pubKey = crypto.getPublicKey();
    newKey.privKey.reset(new AES256(password, crypto.getPrivateKey()));

    keys.insert(newKey);

    return newKey;
}

bool CryptoKernel::Wallet::Account::operator<(const CryptoKernel::Wallet::Account& rhs)
const {
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
    returning["value"] = value;

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

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByName(
    const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    const Json::Value accountJson = accounts->get(dbTx.get(), name);
    if(accountJson.isObject()) {
        return Account(accountJson);
    } else {
        throw WalletException("Account not found for given name");
    }
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByKey(
    const std::string& pubKey) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    return getAccountByKey(dbTx.get(), pubKey);
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::getAccountByKey(
    CryptoKernel::Storage::Transaction* dbTx, const std::string& pubKey) {
    const Json::Value accName = accounts->get(dbTx, pubKey, 0);
    if(!accName.isNull()) {
        const Account acc = Account(accounts->get(dbTx, accName.asString()));
        return acc;
    }

    throw WalletException("Account not found for given pubKey");
}

CryptoKernel::Wallet::Account CryptoKernel::Wallet::newAccount(const std::string& name,
                                                               const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    try {
        getAccountByName(name);
    } catch(const WalletException& e) {
        if(!checkPassword(password)) {
            throw WalletException("Incorrect wallet password");
        }

        const Account acc = Account(name, password);
        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
        accounts->put(dbTx.get(), name, acc.toJson());
        accounts->put(dbTx.get(), (*acc.getKeys().begin()).pubKey, name, 0);
        dbTx->commit();
        return acc;
    }

    throw WalletException("Account already exists");
}

std::string CryptoKernel::Wallet::sendToAddress(const std::string& pubKey,
        const uint64_t amount, const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Transaction> bchainTx(blockchain->getTxHandle());

    if(!checkPassword(password)) {
        return "Incorrect wallet password";
    }

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

    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(
        utxos.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const Txo out = Txo(it->value());
        if(accumulator < amount + fee) {
            if(!out.isSpent()) {
                try {
                    const CryptoKernel::Blockchain::output fullOut = blockchain->getOutput(bchainTx.get(),
                            it->key());
                    if(fullOut.getData()["contract"].isNull()) {
                        fee += CryptoKernel::Storage::toString(fullOut.getData()).size() * 60;
                        toSpend.insert(fullOut);
                        accumulator += fullOut.getValue();
                    }
                } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                    continue;
                }
            }
        } else {
            break;
        }
    }
    delete it;

    if(accumulator < amount + fee) {
        return "Insufficient funds when " + std::to_string(fee / 100000000.0) +
               " fee is included";
    }

    std::uniform_int_distribution<uint64_t> distribution(0,
            std::numeric_limits<uint64_t>::max());
    Json::Value data;
    data["publicKey"] = pubKey;

    const CryptoKernel::Blockchain::output toThem = CryptoKernel::Blockchain::output(amount,
            distribution(generator), data);

    std::stringstream buffer;
    buffer << distribution(generator) << "_change";
    const Account account = newAccount(buffer.str(), password);

    data.clear();
    data["publicKey"] = (*(account.getKeys().begin())).pubKey;

    const CryptoKernel::Blockchain::output change = CryptoKernel::Blockchain::output(
                accumulator - amount - fee, distribution(generator), data);

    std::set<CryptoKernel::Blockchain::output> outputs;
    outputs.insert(change);
    outputs.insert(toThem);

    const std::string outputHash = CryptoKernel::Blockchain::transaction::getOutputSetId(
                                       outputs).toString();

    std::set<CryptoKernel::Blockchain::input> spends;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    for(const CryptoKernel::Blockchain::output& out : toSpend) {
        const std::string publicKey = out.getData()["publicKey"].asString();
        Account acc = getAccountByKey(dbTx.get(), publicKey);

        std::string privKey = "";

        for(const auto& key : acc.getKeys()) {
            if(key.pubKey == publicKey) {
                privKey = key.privKey->decrypt(password);
                break;
            }
        }

        crypto.setPrivateKey(privKey);

        const std::string signature = crypto.sign(out.getId().toString() + outputHash);
        Json::Value spendData;
        spendData["signature"] = signature;

        const CryptoKernel::Blockchain::input inp = CryptoKernel::Blockchain::input(out.getId(),
                spendData);
        spends.insert(inp);

        Txo utxo = Txo(utxos->get(dbTx.get(), out.getId().toString()));
        utxo.spend();
        utxos->put(dbTx.get(), out.getId().toString(), utxo.toJson());
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::transaction(
                spends, outputs, now);

    bchainTx->abort();

    if(!std::get<0>(blockchain->submitTransaction(tx))) {
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
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new
            CryptoKernel::Storage::Table::Iterator(utxos.get(), walletdb.get()));

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
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new
            CryptoKernel::Storage::Table::Iterator(accounts.get(), walletdb.get()));

    std::set<CryptoKernel::Wallet::Account> returning;

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        const Account account = Account(it->value());
        returning.insert(account);
    }

    return returning;
}


// first element of tuple will have the confirmed, second will have unconfirmed Txs
std::tuple<std::set<CryptoKernel::Blockchain::transaction>, std::set<CryptoKernel::Blockchain::transaction>> CryptoKernel::Wallet::listTransactions() {
    std::lock_guard<std::recursive_mutex> lock(walletLock);
    std::unique_ptr<CryptoKernel::Storage::Table::Iterator> it(new
            CryptoKernel::Storage::Table::Iterator(transactions.get(), walletdb.get()));

    std::tuple<std::set<CryptoKernel::Blockchain::transaction>, std::set<CryptoKernel::Blockchain::transaction>> returning;

    const auto& unconfirmedTxs = blockchain->getUnconfirmedTransactions();

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        if(it->value()["unconfirmed"].asBool()) {
            for (const CryptoKernel::Blockchain::transaction& tx : unconfirmedTxs) {
                if (tx.getId().toString() == it->key()) {
                   std::get<1>(returning).insert(tx); 
                   break;
                }
            }
        }
        else {
            const CryptoKernel::Blockchain::transaction tx = blockchain->getTransaction(it->key());
            std::get<0>(returning).insert(tx);
        }
    }
    return returning;
}

std::string CryptoKernel::Wallet::signMessage(const std::string& message,
                                              const std::string& publicKey,
                                              const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    if(!checkPassword(password)) {
        throw WalletException("Incorrect wallet password");
    }

    const Account acc = getAccountByKey(publicKey);
    
    std::string privKey;

    for(const auto& key : acc.getKeys()) {
        if(key.pubKey == publicKey) {
            privKey = key.privKey->decrypt(password);
            break;
        }
    }

    CryptoKernel::Crypto crypto;
    crypto.setPrivateKey(privKey);

    return crypto.sign(message);
}

CryptoKernel::Blockchain::transaction
CryptoKernel::Wallet::signTransaction(const CryptoKernel::Blockchain::transaction& tx,
                                      const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    if(!checkPassword(password)) {
        throw WalletException("Incorrect wallet password");
    }

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
                privKey = key.privKey->decrypt(password);
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

CryptoKernel::Wallet::Account
CryptoKernel::Wallet::importPrivKey(const std::string& name, const std::string& privKey,
                                    const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(walletLock);

    if(!checkPassword(password)) {
        throw WalletException("Incorrect wallet password");
    }

    bool accountExists = false;

    try {
        const Account acc = getAccountByName(name);
        accountExists = true;
    } catch(const WalletException& e) {}

    CryptoKernel::Crypto crypto;
    if(!crypto.setPrivateKey(privKey)) {
        throw WalletException("Invalid private key");
    }

    try {
        const Account acc = getAccountByKey(crypto.getPublicKey());
    } catch(const WalletException& e) {
        Account::keyPair kp;
        kp.privKey.reset(new AES256(password, crypto.getPrivateKey()));
        kp.pubKey = crypto.getPublicKey();

        if(!accountExists) {
            newAccount(name, password);
        }

        Account acc = getAccountByName(name);
        acc.addKeyPair(kp);

        std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
        accounts->put(dbTx.get(), name, acc.toJson());
        accounts->put(dbTx.get(), kp.pubKey, name, 0);
        dbTx->commit();

        // Rescan
        clearDB();

        return acc;
    }

    throw WalletException("Private key already in wallet");
}

// This portion of code is from the following article: http://www.cplusplus.com/articles/E6vU7k9E/
#ifdef _WIN32
std::string getPass(const char *prompt, bool show_asterisk) {
    const char BACKSPACE = 8;
    const char RETURN = 13;

    std::string password;
    unsigned char ch = 0;

    std::cout << prompt;

    DWORD con_mode;
    DWORD dwRead;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleMode(hIn, &con_mode);
    SetConsoleMode(hIn, con_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

    while(ReadConsoleA(hIn, &ch, 1, &dwRead, NULL) && ch != RETURN) {
        if(ch == BACKSPACE) {
            if(password.length()!=0) {
                if(show_asterisk) {
                    std::cout << "\b \b";
                }
                password.resize(password.length()-1);
            }
        } else {
            password += ch;
            if(show_asterisk) {
                std::cout <<'*';
            }
        }
    }

    std::cout << std::endl;

    return password;
}
#else
int getch() {
    int ch;
    struct termios t_old, t_new;

    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

std::string getPass(const char *prompt, bool show_asterisk) {
    const char BACKSPACE = 127;
    const char RETURN = 10;

    std:: string password;
    unsigned char ch = 0;

    std::cout << prompt;

    while((ch=getch()) != RETURN) {
       if(ch == BACKSPACE) {
            if(password.length()!=0) {
                 if(show_asterisk) {
                     std::cout << "\b \b";
                 }
                 password.resize(password.length()-1);
            }
         } else {
             password += ch;
             if(show_asterisk) {
                 std::cout << '*';
             }
         }
    }

    std::cout << std::endl;

    return password;
}
#endif
