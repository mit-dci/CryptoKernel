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
    returning["height"] = blockchain->getBlock("tip").height;
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
    const CryptoKernel::Blockchain::transaction transaction = CryptoKernel::Blockchain::jsonToTransaction(tx);
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

    const std::vector<CryptoKernel::Blockchain::output> utxos = blockchain->getUnspentOutputs(addr.publicKey);

    Json::Value returning;

    for(CryptoKernel::Blockchain::output output : utxos)
    {
        returning["outputs"].append(CryptoKernel::Blockchain::outputToJson(output));
    }

    return returning;
}

std::string CryptoServer::compilecontract(const std::string& code)
{
    return CryptoKernel::ContractRunner::compile(code);
}

std::string CryptoServer::calculateoutputid(const Json::Value output)
{
    const CryptoKernel::Blockchain::output out = CryptoKernel::Blockchain::jsonToOutput(output);
    return CryptoKernel::Blockchain::calculateOutputId(out);
}

Json::Value CryptoServer::signtransaction(const Json::Value tx)
{
    const CryptoKernel::Blockchain::transaction transaction = CryptoKernel::Blockchain::jsonToTransaction(tx);

    return CryptoKernel::Blockchain::transactionToJson(wallet->signTransaction(transaction));
}

Json::Value CryptoServer::listtransactions() {
    Json::Value returning;

    const std::set<CryptoKernel::Blockchain::transaction> transactions = wallet->listTransactions();
    for(const CryptoKernel::Blockchain::transaction tx : transactions) {
        returning.append(CryptoKernel::Blockchain::transactionToJson(tx));
    }

    return returning;
}
