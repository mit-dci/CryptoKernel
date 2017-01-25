#include "version.h"
#include "networkpeer.h"

CryptoKernel::Network::Peer::Peer(sf::TcpSocket* client, CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network)
{
    this->client = client;
    this->blockchain = blockchain;
    this->network = network;
    running = true;

    requestThread.reset(new std::thread(&CryptoKernel::Network::Peer::requestFunc, this));
}

CryptoKernel::Network::Peer::~Peer()
{
    running = false;
    requestThread->join();
    client->disconnect();
    delete client;
}

Json::Value CryptoKernel::Network::Peer::sendRecv(const Json::Value request)
{
    sf::Packet packet;
    packet << request.toStyledString();

    clientMutex.lock();

    client->setBlocking(true);

    if(client->send(packet) != sf::Socket::Done)
    {
        throw NetworkError();
    }

    packet.clear();

    if(client->receive(packet) != sf::Socket::Done)
    {
        throw NetworkError();
    }

    clientMutex.unlock();

    std::string responseString;
    packet >> responseString;

    const Json::Value returning = CryptoKernel::Storage::toJson(responseString);

    return returning;
}

void CryptoKernel::Network::Peer::send(const Json::Value response)
{
    sf::Packet packet;
    packet << response.toStyledString();

    client->setBlocking(true);
    client->send(packet);
}

void CryptoKernel::Network::Peer::requestFunc()
{
    while(running)
    {
        clientMutex.lock();
        sf::Packet packet;
        client->setBlocking(false);

        if(client->receive(packet) != sf::Socket::NotReady)
        {
            std::string requestString;
            packet >> requestString;

            const Json::Value request = CryptoKernel::Storage::toJson(requestString);

            if(request["command"] == "info")
            {
                Json::Value response;
                response["version"] = version;
                response["tipHeight"] = blockchain->getBlock("tip").height;
                send(response);
            }
            else if(request["command"] == "transactions")
            {
                std::vector<CryptoKernel::Blockchain::transaction> txs;
                for(unsigned int i = 0; i < request["data"].size(); i++)
                {
                    const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::jsonToTransaction(request["data"][i]);
                    if(blockchain->submitTransaction(tx))
                    {
                        txs.push_back(tx);
                    }
                }

                clientMutex.unlock();

                network->broadcastTransactions(txs);
            }
            else if(request["command"] == "block")
            {
                const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::jsonToBlock(request["data"]);

                bool blockExists = (blockchain->getBlock(block.id).id == block.id);

                if(!blockExists)
                {
                    if(blockchain->submitBlock(block))
                    {
                        clientMutex.unlock();
                        network->broadcastBlock(block);
                    }
                }
            }
            else if(request["command"] == "getunconfirmed")
            {
                const std::vector<CryptoKernel::Blockchain::transaction> unconfirmedTransactions = blockchain->getUnconfirmedTransactions();
                Json::Value response;
                for(CryptoKernel::Blockchain::transaction tx : unconfirmedTransactions)
                {
                    response.append(CryptoKernel::Blockchain::transactionToJson(tx));
                }

                send(response);
            }
            else if(request["command"] == "getblocks")
            {
                const uint64_t start = request["data"]["start"].asUInt64();
                const uint64_t end = request["data"]["end"].asUInt64();
                std::vector<CryptoKernel::Blockchain::block> blocks;
                if(end > start && (end - start) <= 500)
                {
                    CryptoKernel::Blockchain::block currentBlock = blockchain->getBlock("tip");
                    while(currentBlock.height > 0 && currentBlock.height > end)
                    {
                        currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
                    }

                    while(currentBlock.height >= start && currentBlock.height > 0)
                    {
                        blocks.push_back(currentBlock);
                        currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
                    }

                    std::reverse(blocks.begin(), blocks.end());

                    Json::Value returning;
                    for(CryptoKernel::Blockchain::block block : blocks)
                    {
                        returning["data"].append(CryptoKernel::Blockchain::blockToJson(block));
                    }

                    send(returning);
                }
                else
                {
                    send(Json::Value());
                }
            }
            else if(request["command"] == "getblock")
            {
                if(request["data"]["id"].empty())
                {
                    CryptoKernel::Blockchain::block currentBlock = blockchain->getBlock("tip");
                    while(currentBlock.height > 0 && currentBlock.height != request["data"]["height"].asUInt64())
                    {
                        currentBlock = blockchain->getBlock(currentBlock.previousBlockId);
                    }

                    send(CryptoKernel::Blockchain::blockToJson(currentBlock));
                }
                else
                {
                    send(CryptoKernel::Blockchain::blockToJson(blockchain->getBlock(request["data"]["id"].asString())));
                }
            }
        }

        clientMutex.unlock();
    }
}

Json::Value CryptoKernel::Network::Peer::getInfo()
{
    Json::Value request;
    request["command"] = "info";

    return sendRecv(request);
}

void CryptoKernel::Network::Peer::sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction> transactions)
{
    Json::Value request;
    request["command"] = "transactions";
    for(CryptoKernel::Blockchain::transaction tx : transactions)
    {
        request["data"].append(CryptoKernel::Blockchain::transactionToJson(tx));
    }

    clientMutex.lock();
    send(request);
    clientMutex.unlock();
}

void CryptoKernel::Network::Peer::sendBlock(const CryptoKernel::Blockchain::block block)
{
    Json::Value request;
    request["command"] = "block";
    request["data"] = CryptoKernel::Blockchain::blockToJson(block);

    clientMutex.lock();
    send(request);
    clientMutex.unlock();
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Network::Peer::getUnconfirmedTransactions()
{
    Json::Value request;
    request["command"] = "getunconfirmed";
    Json::Value unconfirmed = sendRecv(request);

    std::vector<CryptoKernel::Blockchain::transaction> returning;
    for(unsigned int i = 0; i < unconfirmed.size(); i++)
    {
        returning.push_back(CryptoKernel::Blockchain::jsonToTransaction(unconfirmed[i]));
    }

    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Peer::getBlock(const uint64_t height, const std::string id)
{
    Json::Value request;
    request["command"] = "getblock";
    Json::Value block;
    if(id != "")
    {
        request["data"]["id"] = id;
        block = sendRecv(request);
    }
    else
    {
        request["data"]["height"] = static_cast<unsigned long long int>(height);
        block = sendRecv(request);
    }

    return CryptoKernel::Blockchain::jsonToBlock(block);
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Peer::getBlocks(const uint64_t start, const uint64_t end)
{
    Json::Value request;
    request["command"] = "getblocks";
    request["data"]["start"] = static_cast<unsigned long long int>(start);
    request["data"]["end"] = static_cast<unsigned long long int>(end);
    Json::Value blocks = sendRecv(request)["data"];

    std::vector<CryptoKernel::Blockchain::block> returning;
    for(unsigned int i = 0; i < blocks.size(); i++)
    {
        returning.push_back(CryptoKernel::Blockchain::jsonToBlock(blocks[i]));
    }

    return returning;
}
