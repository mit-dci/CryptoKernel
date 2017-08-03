/*  CryptoCurrency - An example crypto-currency using the CryptoKernel library as a base
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

#include <sstream>
#include <iomanip>

#include "cryptoserver.h"
#include "version.h"
#include "contract.h"

CryptoServer::CryptoServer(jsonrpc::AbstractServerConnector &connector) : CryptoRPCServer(
        connector) {

}

void CryptoServer::setWallet(CryptoKernel::Wallet* Wallet,
                             CryptoKernel::Blockchain* Blockchain, 
                             CryptoKernel::Network* Network, 
                             bool* running) {
    wallet = Wallet;
    blockchain = Blockchain;
    network = Network;
    this->running = running;
}

Json::Value CryptoServer::getinfo() {
    Json::Value returning;

    returning["rpc_version"] = "2.0.0-dev";
    returning["ck_version"] = version;
    double balance = wallet->getTotalBalance() / 100000000.0;
    std::stringstream buffer;
    buffer << std::setprecision(8) << balance;
    returning["balance"] = buffer.str();
    returning["height"] = static_cast<unsigned long long int>
                          (blockchain->getBlockDB("tip").getHeight());
    returning["connections"] = network->getConnections();
    returning["mempool"]["count"] = blockchain->mempoolCount();
    
    buffer.str("");
    buffer << std::setprecision(3) 
           << (blockchain->mempoolSize() / double(1024 * 1024)) 
           << " MB";
    
    returning["mempool"]["size"] = buffer.str();

    return returning;
}

Json::Value CryptoServer::account(const std::string& account,
                                  const std::string& password) {
    Json::Value returning;
    Json::Value accJson;

    try {
        accJson = wallet->getAccountByName(account).toJson();
    } catch(const CryptoKernel::Wallet::WalletException& e) {
        try {
            accJson = wallet->newAccount(account, password).toJson();
        } catch(const std::runtime_error& e) {
            return Json::nullValue;
        }
    }   
    
    const auto newAccount = CryptoKernel::Wallet::Account(accJson);

    returning["name"] = newAccount.getName();
    double balance = newAccount.getBalance() / 100000000.0;
    std::stringstream buffer;
    buffer << std::setprecision(8) << balance;
    returning["balance"] = buffer.str();

    for(const auto& addr : newAccount.getKeys()) {
        Json::Value keyPair;
        keyPair["pubKey"] = addr.pubKey;
        keyPair["privKey"] = addr.privKey->toJson();
        returning["keys"].append(keyPair);
    }

    return returning;
}

std::string CryptoServer::sendtoaddress(const std::string& address, double amount, 
                                        const std::string& password) {
    uint64_t Amount = amount * 100000000;
    return wallet->sendToAddress(address, Amount, password);
}

bool CryptoServer::sendrawtransaction(const Json::Value tx) {
    try {
        const CryptoKernel::Blockchain::transaction transaction =
            CryptoKernel::Blockchain::transaction(tx);
        if(std::get<0>(blockchain->submitTransaction(transaction))) {
            std::vector<CryptoKernel::Blockchain::transaction> txs;
            txs.push_back(transaction);
            network->broadcastTransactions(txs);
            return true;
        }
    } catch(CryptoKernel::Blockchain::InvalidElementException e) {
        return false;
    }

    return false;
}

Json::Value CryptoServer::listaccounts() {
    Json::Value returning;

    const std::set<CryptoKernel::Wallet::Account> accounts = wallet->listAccounts();

    returning["accounts"] = Json::Value();

    for(const auto& acc : accounts) {
        Json::Value account;
        account["name"] = acc.getName();
        double balance = acc.getBalance() / 100000000.0;
        std::stringstream buffer;
        buffer << std::setprecision(8) << balance;
        account["balance"] = buffer.str();

        for(const auto& addr : acc.getKeys()) {
            Json::Value keyPair;
            keyPair["pubKey"] = addr.pubKey;
            keyPair["privKey"] = addr.privKey->toJson();
            account["keys"].append(keyPair);
        }

        returning["accounts"].append(account);
    }

    return returning;
}

Json::Value CryptoServer::listunspentoutputs(const std::string& account) {
    try {
        CryptoKernel::Wallet::Account acc = wallet->getAccountByName(account);

        std::set<CryptoKernel::Blockchain::output> utxos;

        for(const auto& addr : acc.getKeys()) {
            const auto unspent = blockchain->getUnspentOutputs(addr.pubKey);
            utxos.insert(unspent.begin(), unspent.end());
        }

        Json::Value returning;

        for(const CryptoKernel::Blockchain::output& output : utxos) {
            Json::Value out = output.toJson();
            out["id"] = output.getId().toString();
            returning["outputs"].append(out);
        }

        return returning;
    } catch(const CryptoKernel::Wallet::WalletException& e) {
        return Json::Value();
    }
}

std::string CryptoServer::compilecontract(const std::string& code) {
    return CryptoKernel::ContractRunner::compile(code);
}

std::string CryptoServer::calculateoutputid(const Json::Value output) {
    try {
        const CryptoKernel::Blockchain::output out = CryptoKernel::Blockchain::output(output);
        return out.getId().toString();
    } catch(CryptoKernel::Blockchain::InvalidElementException e) {
        return "";
    }
}

Json::Value CryptoServer::signtransaction(const Json::Value& tx, const std::string& password) {
    try {
        const CryptoKernel::Blockchain::transaction transaction =
            CryptoKernel::Blockchain::transaction(tx);

        return wallet->signTransaction(transaction, password).toJson();
    } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
        return Json::Value();
    } catch(const std::runtime_error& e) {
        return Json::Value();
    }
}

Json::Value CryptoServer::listtransactions() {
    Json::Value returning;

    returning["transactions"] = Json::Value();

    const std::set<CryptoKernel::Blockchain::transaction> transactions =
        wallet->listTransactions();

    for(const auto& tx : transactions) {
        returning["transactions"].append(tx.toJson());
    }

    return returning;
}

Json::Value CryptoServer::getblockbyheight(const uint64_t height) {
    try {
        const CryptoKernel::Blockchain::block block = blockchain->getBlockByHeight(height);
        Json::Value returning = block.toJson();
        returning["id"] = block.getId().toString();
        return returning;
    } catch(CryptoKernel::Blockchain::NotFoundException e) {
        return Json::Value();
    }
}

bool CryptoServer::stop() {
    *running = false;
    return true;
}

Json::Value CryptoServer::getblock(const std::string& id) {
    try {
        return blockchain->getBlock(id).toJson();
    } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
        return Json::Value();
    }
}

Json::Value CryptoServer::gettransaction(const std::string& id) {
    try {
        return blockchain->getTransaction(id).toJson();
    } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
        return Json::Value();
    }
}

Json::Value CryptoServer::importprivkey(const std::string& name, const std::string& key,
                                        const std::string& password) {
    try {
        return wallet->importPrivKey(name, key, password).toJson();
    } catch(const CryptoKernel::Wallet::WalletException& e) {
        return Json::Value();
    } catch(const std::runtime_error& e) {
        return Json::Value();
    }
}
