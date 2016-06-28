#include <ctime>
#include <sstream>
#include <algorithm>

#include "blockchain.h"
#include "crypto.h"

CryptoKernel::Blockchain::Blockchain()
{
    transactions = new CryptoKernel::Storage("./transactiondb");
    blocks = new CryptoKernel::Storage("./blockdb");
    utxos = new CryptoKernel::Storage("./utxodb");
    currency = new CryptoKernel::Currency();
}

CryptoKernel::Blockchain::~Blockchain()
{
    delete transactions;
    delete blocks;
    delete currency;
}

double CryptoKernel::Blockchain::getBalance(std::string publicKey)
{
    return currency->getBalance(publicKey);
}

bool CryptoKernel::Blockchain::verifyTransaction(transaction tx)
{
    if(calculateTransactionId(tx) != tx.id)
    {
        return false;
    }

    time_t t = std::time(0);
    unsigned int now = static_cast<unsigned int> (t);
    if(tx.timestamp < (now - 60 * 60))
    {
        return false;
    }

    double inputTotal = 0;
    double outputTotal = 0;
    std::vector<output>::iterator it;

    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        inputTotal += (*it).value;
        CryptoKernel::Crypto crypto;
        crypto.setPublicKey((*it).publicKey);
        if(!crypto.verify((*it).id, (*it).signature))
        {
            return false;
        }
        else
        {
            //Check if input has already been spent
            if(utxos->get((*it).id)["id"] == "")
            {
                return false;
            }
        }
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        outputTotal += (*it).value;
    }

    if(outputTotal != inputTotal)
    {
        return false;
    }

    return true;
}

bool CryptoKernel::Blockchain::submitTransaction(transaction tx)
{
    if(verifyTransaction(tx))
    {
        if(transactions->get(tx.id)["id"] != "")
        {
            //Transaction has already been submitted and verified
            return false;
        }
        else
        {
            //Transaction has not already been verified

            //Check if transaction is already in the unconfirmed vector
            std::vector<transaction>::iterator it;
            it = std::find_if(unconfirmedTransactions.begin(), unconfirmedTransactions.end(), [&](const transaction & utx)
            {
                return (utx.id == tx.id);
            });

            if(it->id == "")
            {
                unconfirmedTransactions.push_back(tx);
                return true;
            }
            else
            {
                //Transaction is already in the unconfirmed vector
                return false;
            }
        }
    }
    else
    {
        return false;
    }
}

Json::Value CryptoKernel::Blockchain::transactionToJson(transaction tx)
{
    Json::Value returning;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        returning["inputs"].append(outputToJson((*it)));
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        returning["inputs"].append(outputToJson((*it)));
    }

    returning["timestamp"] = tx.timestamp;
    returning["id"] = tx.id;

    return returning;
}

Json::Value CryptoKernel::Blockchain::outputToJson(output Output)
{
    Json::Value returning;

    returning["id"] = Output.id;
    returning["value"] = Output.value;
    returning["signature"] = Output.signature;
    returning["publicKey"] = Output.publicKey;
    returning["data"] = Output.data;

    return returning;
}

std::string CryptoKernel::Blockchain::calculateTransactionId(transaction tx)
{
    std::stringstream buffer;

    std::vector<output>::iterator it;
    for(it = tx.inputs.begin(); it < tx.inputs.end(); it++)
    {
        buffer << calculateOutputId((*it));
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        buffer << calculateOutputId((*it));
    }

    buffer << tx.timestamp;

    CryptoKernel::Crypto crypto;

    return crypto.sha256(buffer.str());
}

std::string CryptoKernel::Blockchain::calculateOutputId(output Output)
{
    std::stringstream buffer;

    buffer << Output.publicKey << Output.value << CryptoKernel::Storage::toString(Output.data);

    CryptoKernel::Crypto crypto;
    return crypto.sha256(buffer.str());
}


