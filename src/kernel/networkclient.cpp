#include "networkclient.h"

CryptoKernel::Network::Client::Client(const std::string url)
{
    httpClient.reset(new jsonrpc::HttpClient(url));
    client.reset(new NetworkClient(*httpClient.get()));
}

CryptoKernel::Network::Client::~Client()
{

}

Json::Value CryptoKernel::Network::Client::getInfo()
{
    return client->info();
}

void CryptoKernel::Network::Client::sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction> transactions)
{
    Json::Value jsonTxs;
    for(CryptoKernel::Blockchain::transaction tx : transactions)
    {
        jsonTxs.append(CryptoKernel::Blockchain::transactionToJson(tx));
    }

    client->transactions(jsonTxs);
}

void CryptoKernel::Network::Client::sendBlock(const CryptoKernel::Blockchain::block block)
{
    client->block(CryptoKernel::Blockchain::blockToJson(block));
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Network::Client::getUnconfirmedTransactions()
{
    std::vector<CryptoKernel::Blockchain::transaction> returning;
    Json::Value unconfirmed = client->getunconfirmed();
    for(unsigned int i = 0; i < unconfirmed.size(); i++)
    {
        returning.push_back(CryptoKernel::Blockchain::jsonToTransaction(unconfirmed[i]));
    }

    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Client::getBlock(const uint64_t height, const std::string id)
{
    if(id != "")
    {
        Json::Value block = client->getblock(0, id);
        return CryptoKernel::Blockchain::jsonToBlock(block);
    }
    else
    {
        Json::Value block = client->getblock(static_cast<uint64_t>(height), "");
        return CryptoKernel::Blockchain::jsonToBlock(block);
    }
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Client::getBlocks(const int start, const int end)
{
    std::vector<CryptoKernel::Blockchain::block> returning;
    Json::Value blocks = client->getblocks(end, start)["blocks"];
    for(unsigned int i = 0; i < blocks.size(); i++)
    {
        returning.push_back(CryptoKernel::Blockchain::jsonToBlock(blocks[i]));
    }

    return returning;
}
