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
#include <algorithm>

#include "crypto.h"
#include "contract.h"
#include "wallet.h"

CryptoCurrency::Wallet::Wallet(CryptoKernel::Blockchain* Blockchain, CryptoKernel::Network* network)
{
    blockchain = Blockchain;
    this->network = network;

    walletdb.reset(new CryptoKernel::Storage("./addressesdb"));
    addresses.reset(new CryptoKernel::Storage::Table("addresses"));

    const time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));
}

CryptoCurrency::Wallet::~Wallet()
{

}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::newAddress(std::string name)
{
    address Address;
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());
    if(addresses->get(dbTx.get(), name)["name"].asString() != name)
    {
        CryptoKernel::Crypto crypto(true);

        Address.name = name;
        Address.publicKey = crypto.getPublicKey();
        Address.privateKey = crypto.getPrivateKey();
        Address.balance = 0;

        addresses->put(dbTx.get(), name, addressToJson(Address));
        addresses->put(dbTx.get(), Address.publicKey, Json::Value(name), 0);
    }

    dbTx->commit();

    return Address;
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::getAddressByName(std::string name)
{
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    return jsonToAddress(addresses->get(dbTx.get(), name));
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::getAddressByKey(const std::string publicKey)
{
    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    const std::string name = addresses->get(dbTx.get(), publicKey, 0).asString();

    return jsonToAddress(addresses->get(dbTx.get(), name));
}

Json::Value CryptoCurrency::Wallet::addressToJson(address Address)
{
    Json::Value returning;

    returning["name"] = Address.name;
    returning["publicKey"] = Address.publicKey;
    returning["privateKey"] = Address.privateKey;
    returning["balance"] = static_cast<unsigned long long int>(Address.balance);

    return returning;
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::jsonToAddress(Json::Value Address)
{
    address returning;

    returning.name = Address["name"].asString();
    returning.publicKey = Address["publicKey"].asString();
    returning.privateKey = Address["privateKey"].asString();
    returning.balance = Address["balance"].asUInt64();

    return returning;
}

bool CryptoCurrency::Wallet::updateAddressBalance(CryptoKernel::Storage::Transaction* dbTx, std::string name, uint64_t amount)
{
    address Address;
    Address = jsonToAddress(addresses->get(dbTx, name));
    if(Address.name == name)
    {
        Address.balance = amount;
        addresses->put(dbTx, name, addressToJson(Address));
        return true;
    }
    else
    {
        return false;
    }
}

std::string CryptoCurrency::Wallet::sendToAddress(const std::string& publicKey, const uint64_t amount)
{
    if(getTotalBalance() < amount) {
        return "Insufficient funds";
    }

    CryptoKernel::Crypto crypto;
    if(!crypto.setPublicKey(publicKey)) {
        return "Invalid address";
    }

    std::set<CryptoKernel::Blockchain::output> inputs;
    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(addresses.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        std::set<CryptoKernel::Blockchain::output> tempInputs;
        tempInputs = blockchain->getUnspentOutputs(it->value()["publicKey"].asString());
        for(const CryptoKernel::Blockchain::output& out : tempInputs) {
            if(out.getData()["contract"].empty()) {
                inputs.insert(out);
            }
        }
    }
    delete it;

    std::set<CryptoKernel::Blockchain::output> toSpend;
    uint64_t accumulator = 0;
    uint64_t fee = 150000;
    for(const CryptoKernel::Blockchain::output& out : inputs) {
        if(accumulator < amount + fee) {
            fee += CryptoKernel::Storage::toString(out.getData()).size() * 600;
            toSpend.insert(out);
            accumulator += out.getValue();
        } else {
            break;
        }
    }

    if(accumulator < amount + fee) {
        return "Insufficient funds when " + std::to_string(fee / 100000000.0) + " fee is included";
    }

    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());
    Json::Value data;
    data["publicKey"] = publicKey;

    const CryptoKernel::Blockchain::output toThem = CryptoKernel::Blockchain::output(amount, distribution(generator), data);

    std::stringstream buffer;
    buffer << distribution(generator) << "_change";
    const address Address = newAddress(buffer.str());

    data.clear();
    data["publicKey"] = Address.publicKey;

    const CryptoKernel::Blockchain::output change = CryptoKernel::Blockchain::output(accumulator - amount - fee, distribution(generator), data);

    std::set<CryptoKernel::Blockchain::output> outputs;
    outputs.insert(change);
    outputs.insert(toThem);

    const std::string outputHash = CryptoKernel::Blockchain::transaction::getOutputSetId(outputs).toString();

    std::set<CryptoKernel::Blockchain::input> spends;

    for(const CryptoKernel::Blockchain::output& out : toSpend) {
        address Address = getAddressByKey(out.getData()["publicKey"].asString());
        crypto.setPrivateKey(Address.privateKey);

        const std::string signature = crypto.sign(out.getId().toString() + outputHash);
        Json::Value spendData;
        spendData["signature"] = signature;

        const CryptoKernel::Blockchain::input inp = CryptoKernel::Blockchain::input(out.getId(), spendData);
        spends.insert(inp);
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::transaction(spends, outputs, now);

    if(!blockchain->submitTransaction(tx))
    {
        return "Error submitting transaction";
    }

    std::vector<CryptoKernel::Blockchain::transaction> txs;
    txs.push_back(tx);
    network->broadcastTransactions(txs);

    return tx.getId().toString();
}

double CryptoCurrency::Wallet::getTotalBalance()
{
    rescan();

    double balance = 0;

    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(addresses.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        balance += it->value()["balance"].asDouble();
    }
    delete it;

    return balance;
}

void CryptoCurrency::Wallet::rescan()
{
    std::vector<address> tempAddresses;
    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(addresses.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        address Address;
        Address = jsonToAddress(it->value());
        tempAddresses.push_back(Address);
    }
    delete it;

    std::unique_ptr<CryptoKernel::Storage::Transaction> dbTx(walletdb->begin());

    std::vector<address>::iterator it2;
    for(it2 = tempAddresses.begin(); it2 < tempAddresses.end(); it2++)
    {
        updateAddressBalance(dbTx.get(), (*it2).name, blockchain->getBalance((*it2).publicKey));
    }

    dbTx->commit();
}

std::vector<CryptoCurrency::Wallet::address> CryptoCurrency::Wallet::listAddresses()
{
    rescan();

    std::vector<address> tempAddresses;
    CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(addresses.get(), walletdb.get());
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        address Address;
        Address = jsonToAddress(it->value());
        tempAddresses.push_back(Address);
    }
    delete it;

    return tempAddresses;
}

CryptoKernel::Blockchain::transaction CryptoCurrency::Wallet::signTransaction(const CryptoKernel::Blockchain::transaction tx)
{
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
        const address Address = getAddressByKey(outputData["publicKey"].asString());
        crypto.setPrivateKey(Address.privateKey);
        const std::string signature = crypto.sign(input.getOutputId().toString() + outputHash);
        Json::Value spendData = input.getData();
        spendData["signature"] = signature;
        newInputs.insert(CryptoKernel::Blockchain::input(input.getOutputId(), spendData));
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    return CryptoKernel::Blockchain::transaction(newInputs, tx.getOutputs(), now);
}

std::set<CryptoKernel::Blockchain::transaction> CryptoCurrency::Wallet::listTransactions() {
    /*const std::vector<address> addrs = listAddresses();
    std::set<std::string> pubKeys;
    for(const address addr : addrs) {
        pubKeys.insert(addr.publicKey);
    }

    return blockchain->getTransactionsByPubKeys(pubKeys);*/
}
