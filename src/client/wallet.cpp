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
    log = new CryptoKernel::Log();
    addresses = new CryptoKernel::Storage("./addressesdb");

    rescan();

    const time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));
}

CryptoCurrency::Wallet::~Wallet()
{
    delete log;
    delete addresses;
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::newAddress(std::string name)
{
    address Address;
    if(addresses->get(name)["name"] != name)
    {
        CryptoKernel::Crypto crypto(true);

        Address.name = name;
        Address.publicKey = crypto.getPublicKey();
        Address.privateKey = crypto.getPrivateKey();
        Address.balance = 0;

        addresses->store(name, addressToJson(Address));
    }

    return Address;
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::getAddressByName(std::string name)
{
    address Address;

    Address = jsonToAddress(addresses->get(name));
    updateAddressBalance(Address.name, blockchain->getBalance(Address.publicKey));

    return jsonToAddress(addresses->get(name));
}

CryptoCurrency::Wallet::address CryptoCurrency::Wallet::getAddressByKey(std::string publicKey)
{
    address Address;

    CryptoKernel::Storage::Iterator* it = addresses->newIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        if(it->value()["publicKey"].asString() == publicKey)
        {
            Address = jsonToAddress(it->value());
            break;
        }
    }
    delete it;

    updateAddressBalance(Address.name, blockchain->getBalance(Address.publicKey));

    return jsonToAddress(addresses->get(Address.name));
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

bool CryptoCurrency::Wallet::updateAddressBalance(std::string name, uint64_t amount)
{
    address Address;
    Address = jsonToAddress(addresses->get(name));
    if(Address.name == name)
    {
        Address.balance = amount;
        addresses->store(name, addressToJson(Address));
        return true;
    }
    else
    {
        return false;
    }
}

bool CryptoCurrency::Wallet::sendToAddress(std::string publicKey, uint64_t amount, uint64_t fee)
{
    if(getTotalBalance() < amount + fee)
    {
        return false;
    }

    std::vector<CryptoKernel::Blockchain::output> inputs;
    CryptoKernel::Storage::Iterator* it = addresses->newIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::vector<CryptoKernel::Blockchain::output> tempInputs;
        tempInputs = blockchain->getUnspentOutputs(it->value()["publicKey"].asString());
        std::vector<CryptoKernel::Blockchain::output>::iterator it2;
        for(it2 = tempInputs.begin(); it2 < tempInputs.end(); it2++)
        {
            inputs.push_back(*it2);
        }
    }
    delete it;

    std::vector<CryptoKernel::Blockchain::output> toSpend;

    double accumulator = 0;
    std::vector<CryptoKernel::Blockchain::output>::iterator it2;
    for(it2 = inputs.begin(); it2 < inputs.end(); it2++)
    {
        if(accumulator < amount + fee)
        {
            address Address = getAddressByKey((*it2).publicKey);
            toSpend.push_back(*it2);
            Address.balance -= (*it2).value;
            updateAddressBalance(Address.name, Address.balance);
            accumulator += (*it2).value;
        }
        else
        {
            break;
        }
    }

    CryptoKernel::Blockchain::output toThem;
    toThem.value = amount;

    CryptoKernel::Crypto crypto;
    if(!crypto.setPublicKey(publicKey))
    {
        return false;
    }
    toThem.publicKey = publicKey;

    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());
    toThem.nonce = distribution(generator);

    //const std::string contract = "local json = Json.new() local tx = json:decode(txJson) local crypto = Crypto.new() crypto:setPublicKey(tx[\"inputs\"][1][\"publicKey\"]) if crypto:verify(tx[\"inputs\"][1][\"id\"] .. outputSetId, tx[\"inputs\"][1][\"signature\"]) then return true else return false end";
    //const std::string compressedBytecode = CryptoKernel::ContractRunner::compile(contract);

    Json::Value data;
    //data["contract"] = compressedBytecode;

    toThem.data = data;

    toThem.id = blockchain->calculateOutputId(toThem);

    CryptoKernel::Blockchain::output change;
    change.value = accumulator - amount - fee;

    std::stringstream buffer;
    buffer << distribution(generator) << "_change";
    address Address = newAddress(buffer.str());

    change.publicKey = Address.publicKey;
    change.nonce = distribution(generator);
    change.id = blockchain->calculateOutputId(change);

    std::vector<CryptoKernel::Blockchain::output> outputs;
    outputs.push_back(change);
    outputs.push_back(toThem);
    std::string outputHash = blockchain->calculateOutputSetId(outputs);

    for(it2 = toSpend.begin(); it2 < toSpend.end(); it2++)
    {
        address Address = getAddressByKey((*it2).publicKey);
        crypto.setPrivateKey(Address.privateKey);
        std::string signature = crypto.sign((*it2).id + outputHash);
        (*it2).signature = signature;
    }

    CryptoKernel::Blockchain::transaction tx;
    tx.inputs = toSpend;
    tx.outputs.push_back(toThem);
    tx.outputs.push_back(change);

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    tx.timestamp = now;
    tx.id = blockchain->calculateTransactionId(tx);

    if(!blockchain->submitTransaction(tx))
    {
        return false;
    }

    std::vector<CryptoKernel::Blockchain::transaction> txs;
    txs.push_back(tx);
    network->broadcastTransactions(txs);

    return true;
}

double CryptoCurrency::Wallet::getTotalBalance()
{
    rescan();

    double balance = 0;

    CryptoKernel::Storage::Iterator* it = addresses->newIterator();
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
    CryptoKernel::Storage::Iterator* it = addresses->newIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        address Address;
        Address = jsonToAddress(it->value());
        tempAddresses.push_back(Address);
    }
    delete it;

    std::vector<address>::iterator it2;
    for(it2 = tempAddresses.begin(); it2 < tempAddresses.end(); it2++)
    {
        updateAddressBalance((*it2).name, blockchain->getBalance((*it2).publicKey));
    }
}

std::vector<CryptoCurrency::Wallet::address> CryptoCurrency::Wallet::listAddresses()
{
    rescan();

    std::vector<address> tempAddresses;
    CryptoKernel::Storage::Iterator* it = addresses->newIterator();
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
    CryptoKernel::Blockchain::transaction returning = tx;
    const std::string outputHash = blockchain->calculateOutputSetId(returning.outputs);

    CryptoKernel::Crypto crypto;

    for(CryptoKernel::Blockchain::output& input : returning.inputs)
    {
        const address Address = getAddressByKey(input.publicKey);
        crypto.setPrivateKey(Address.privateKey);
        const std::string signature = crypto.sign(input.id + outputHash);
        input.signature = signature;
    }

    const time_t t = std::time(0);
    const uint64_t now = static_cast<uint64_t> (t);

    returning.timestamp = now;
    returning.id = blockchain->calculateTransactionId(returning);

    return returning;
}

std::set<CryptoKernel::Blockchain::transaction> CryptoCurrency::Wallet::listTransactions() {
    const std::vector<address> addrs = listAddresses();
    std::set<std::string> pubKeys;
    for(const address addr : addrs) {
        pubKeys.insert(addr.publicKey);
    }

    return blockchain->getTransactionsByPubKeys(pubKeys);
}
