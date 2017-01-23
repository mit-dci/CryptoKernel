#include <algorithm>

#include "version.h"
#include "networkserver.h"

CryptoKernel::Network::Server::Server(jsonrpc::AbstractServerConnector &connector) : NetworkServer(connector)
{

}

void CryptoKernel::Network::Server::setBlockchain(CryptoKernel::Blockchain* blockchain)
{
    this->blockchain = blockchain;
}

void CryptoKernel::Network::Server::setNetwork(CryptoKernel::Network* network)
{
    this->network = network;
}

Json::Value CryptoKernel::Network::Server::getblock(const int height, const std::string& id)
{
    if(id != "")
    {
        CryptoKernel::Blockchain::block currentBlock = blockchain->getBlock("tip");
        while(currentBlock.height > 0 && currentBlock.height != (unsigned int)height)
        {
            currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
        }

        return CryptoKernel::Blockchain::blockToJson(currentBlock);
    }
    else
    {
        return CryptoKernel::Blockchain::blockToJson(blockchain->getBlock(id));
    }
}

Json::Value CryptoKernel::Network::Server::getblocks(const int end, const int start)
{
    std::vector<CryptoKernel::Blockchain::block> blocks;
    if(end > start && end > 0 && start > 0 && (end - start) <= 500)
    {
        CryptoKernel::Blockchain::block currentBlock = blockchain->getBlock("tip");
        while(currentBlock.height > 0 && currentBlock.height > (unsigned int)end)
        {
            currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
        }

        while(currentBlock.height >= (unsigned int)start && currentBlock.height > 0)
        {
            blocks.push_back(currentBlock);
            currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
        }

        std::reverse(blocks.begin(), blocks.end());

        Json::Value returning;
        for(CryptoKernel::Blockchain::block block : blocks)
        {
            returning["blocks"].append(CryptoKernel::Blockchain::blockToJson(block));
        }

        return returning;
    }
    else
    {
        return Json::Value();
    }
}

Json::Value CryptoKernel::Network::Server::getunconfirmed()
{
    const std::vector<CryptoKernel::Blockchain::transaction> unconfirmedTransactions = blockchain->getUnconfirmedTransactions();
    Json::Value returning;
    for(CryptoKernel::Blockchain::transaction tx : unconfirmedTransactions)
    {
        returning.append(CryptoKernel::Blockchain::transactionToJson(tx));
    }

    return returning;
}

void CryptoKernel::Network::Server::block(const Json::Value& data)
{
    const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::jsonToBlock(data);
    if(blockchain->submitBlock(block))
    {
        network->broadcastBlock(block);
    }
}

void CryptoKernel::Network::Server::transactions(const Json::Value& data)
{
    std::vector<CryptoKernel::Blockchain::transaction> txs;
    for(unsigned int i = 0; i < data.size(); i++)
    {
        const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::jsonToTransaction(data[i]);
        if(blockchain->submitTransaction(tx))
        {
            txs.push_back(tx);
        }
    }

    network->broadcastTransactions(txs);
}

Json::Value CryptoKernel::Network::Server::info()
{
    Json::Value returning;
    returning["version"] = version;
    returning["tipHeight"] = blockchain->getBlock("tip").height;

    return returning;
}
