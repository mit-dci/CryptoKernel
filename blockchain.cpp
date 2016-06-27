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

bool CryptoKernel::Blockchain::submitTransaction(transaction tx)
{
    //Check that tx hasn't already been sent
    //Check that id sent == actual id
    //Check that timestamp is realistic
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
    }

    for(it = tx.outputs.begin(); it < tx.outputs.end(); it++)
    {
        outputTotal += (*it).value;
    }

    if(outputTotal != inputTotal)
    {
        return false;
    }

    unconfirmedTransactions.push(tx);

    return true;
}


