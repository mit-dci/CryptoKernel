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

CryptoServer::CryptoServer(jsonrpc::AbstractServerConnector &connector) : CryptoRPCServer(connector)
{

}

void CryptoServer::setWallet(CryptoCurrency::Wallet* Wallet, CryptoKernel::Blockchain* Blockchain, CryptoKernel::Network* Network)
{
    wallet = Wallet;
    blockchain = Blockchain;
    network = Network;
}

Json::Value CryptoServer::getinfo()
{
    Json::Value returning;

    returning["RPC Version"] = "1.2.0-dev";
    returning["CK Version"] = version;
    double balance = wallet->getTotalBalance() / 100000000.0;
    std::stringstream buffer;
    buffer << std::setprecision(8) << balance;
    returning["balance"] = buffer.str();
    returning["height"] = static_cast<unsigned long long int>(blockchain->getBlockDB("tip").getHeight());
    returning["connections"] = network->getConnections();

    return returning;
}

Json::Value CryptoServer::account(const std::string& account)
{
    Json::Value returning;

    wallet->newAddress(account);

    CryptoCurrency::Wallet::address newAccount = wallet->getAddressByName(account);

    returning["name"] = newAccount.name;
    double balance = newAccount.balance / 100000000.0;
    std::stringstream buffer;
    buffer << std::setprecision(8) << balance;
    returning["balance"] = buffer.str();
    returning["address"] = newAccount.publicKey;
    returning["privateKey"] = newAccount.privateKey;

    return returning;
}

bool CryptoServer::sendtoaddress(const std::string& address, double amount, double fee)
{
    uint64_t Amount = amount * 100000000;
    uint64_t Fee = fee * 100000000;
    return wallet->sendToAddress(address, Amount, Fee);
}

bool CryptoServer::sendrawtransaction(const Json::Value tx)
{
    const CryptoKernel::Blockchain::transaction transaction = CryptoKernel::Blockchain::transaction(tx);
    if(blockchain->submitTransaction(transaction))
    {
        std::vector<CryptoKernel::Blockchain::transaction> txs;
        txs.push_back(transaction);
        network->broadcastTransactions(txs);
        return true;
    }

    return false;
}

Json::Value CryptoServer::listaccounts()
{
    Json::Value returning;

    const std::vector<CryptoCurrency::Wallet::address> accounts = wallet->listAddresses();

    for(CryptoCurrency::Wallet::address addr : accounts)
    {
        Json::Value account;
        account["name"] = addr.name;
        double balance = addr.balance / 100000000.0;
        std::stringstream buffer;
        buffer << std::setprecision(8) << balance;
        account["balance"] = buffer.str();
        account["address"] = addr.publicKey;

        returning["accounts"].append(account);
    }

    return returning;
}

Json::Value CryptoServer::listunspentoutputs(const std::string& account)
{
    CryptoCurrency::Wallet::address addr = wallet->getAddressByName(account);

    const std::set<CryptoKernel::Blockchain::output> utxos = blockchain->getUnspentOutputs(addr.publicKey);

    Json::Value returning;

    for(const CryptoKernel::Blockchain::output& output : utxos)
    {
        Json::Value out = output.toJson();
        out["id"] = output.getId().toString();
        returning["outputs"].append(out);
    }

    return returning;
}

std::string CryptoServer::compilecontract(const std::string& code)
{
    return CryptoKernel::ContractRunner::compile(code);
}

std::string CryptoServer::calculateoutputid(const Json::Value output)
{
    const CryptoKernel::Blockchain::output out = CryptoKernel::Blockchain::output(output);
    return out.getId().toString();
}

Json::Value CryptoServer::signtransaction(const Json::Value tx)
{
    const CryptoKernel::Blockchain::transaction transaction = CryptoKernel::Blockchain::transaction(tx);

    return wallet->signTransaction(transaction).toJson();
}

Json::Value CryptoServer::listtransactions() {
    Json::Value returning;

    /*const std::set<CryptoKernel::Blockchain::transaction> transactions = wallet->listTransactions();
    std::set<std::string> pubKeys;
    for(const auto address : wallet->listAddresses()) {
        pubKeys.insert(address.publicKey);
    }
    for(const CryptoKernel::Blockchain::transaction tx : transactions) {
        Json::Value transaction;
        transaction["id"] = tx.getId().toString();
        transaction["timestamp"] = static_cast<unsigned long long int>(tx.getTimestamp());

        std::string toThem = "";
        std::string toMe = "";

        bool allToMe = true;

        int64_t delta = 0;
        for(unsigned int i = 0; i < tx.outputs.size(); i++) {
            const CryptoCurrency::Wallet::address address = wallet->getAddressByKey(tx.outputs[i].publicKey);
            if(pubKeys.find(tx.outputs[i].publicKey) != pubKeys.end()) {
                delta += tx.outputs[i].value;
                toMe = tx.outputs[i].publicKey;
            } else {
                toThem = tx.outputs[i].publicKey;
                allToMe = false;
            }
        }

        for(unsigned int i = 0; i < tx.inputs.size(); i++) {
            const CryptoCurrency::Wallet::address address = wallet->getAddressByKey(tx.inputs[i].publicKey);
            if(pubKeys.find(tx.inputs[i].publicKey) != pubKeys.end()) {
                delta -= tx.inputs[i].value;
            }
        }

        const double asFloat = delta / 10000000.0;
        std::stringstream buffer;
        buffer << std::setprecision(8) << asFloat;
        transaction["amount"] = buffer.str();
        if(allToMe) {
            transaction["type"] = "Payment to self";
            transaction["address"] = "N/A";
        } else if(delta < 0) {
            transaction["type"] = "Sent to";
            transaction["address"] = toThem;
        } else if(delta >= 0) {
            transaction["type"] = "Receive";
            transaction["address"] = wallet->getAddressByKey(toMe).name + " (" + toMe + ")";
        }

        returning["transactions"].append(transaction);
    }*/

    return returning;
}

Json::Value CryptoServer::getblockbyheight(const uint64_t height)
{
    return blockchain->getBlockByHeight(height).toJson();
}
