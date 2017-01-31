#include <chrono>

#include "version.h"
#include "networkpeer.h"

CryptoKernel::Network::Peer::Peer(sf::TcpSocket* client, CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network)
{
    this->client = client;
    this->blockchain = blockchain;
    this->network = network;
    running = true;

    time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));

    requestThread.reset(new std::thread(&CryptoKernel::Network::Peer::requestFunc, this));
}

CryptoKernel::Network::Peer::~Peer()
{
    clientMutex.lock();
    running = false;
    requestThread->join();
    client->disconnect();
    delete client;
}

Json::Value CryptoKernel::Network::Peer::sendRecv(const Json::Value request)
{
    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());
    const uint64_t nonce = distribution(generator);

    Json::Value modifiedRequest = request;
    modifiedRequest["nonce"] = static_cast<unsigned long long int>(nonce);

    sf::Packet packet;
    packet << modifiedRequest.toStyledString();

    clientMutex.lock();

    client->setBlocking(true);

    if(client->send(packet) != sf::Socket::Done)
    {
        clientMutex.unlock();
        running = false;
        throw NetworkError();
    }

    clientMutex.unlock();

    for(unsigned int t = 0; t < 120; t++)
    {
        clientMutex.lock();
        std::map<uint64_t, Json::Value>::iterator it = responses.find(nonce);
        if(it != responses.end())
        {
            const Json::Value returning = it->second;
            it = responses.erase(it);
            clientMutex.unlock();
            return returning;
        }
        clientMutex.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    running = false;
    clientMutex.unlock();
    throw NetworkError();
}

void CryptoKernel::Network::Peer::send(const Json::Value response)
{
    sf::Packet packet;
    packet << response.toStyledString();

    clientMutex.lock();
    client->setBlocking(true);
    client->send(packet);
    clientMutex.unlock();
}

void CryptoKernel::Network::Peer::requestFunc()
{
    while(running)
    {
        sf::Packet packet;

        clientMutex.lock();
        client->setBlocking(false);

        if(client->receive(packet) == sf::Socket::Done)
        {
            clientMutex.unlock();
            std::string requestString;
            packet >> requestString;

            const Json::Value request = CryptoKernel::Storage::toJson(requestString);

            if(!request["command"].empty())
            {
                if(request["command"] == "info")
                {
                    Json::Value response;
                    response["data"]["version"] = version;
                    response["data"]["tipHeight"] = blockchain->getBlock("tip").height;
                    response["nonce"] = request["nonce"];
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

                    if(txs.size() > 0)
                    {
                        network->broadcastTransactions(txs);
                    }
                }
                else if(request["command"] == "block")
                {
                    const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::jsonToBlock(request["data"]);

                    bool blockExists = (blockchain->getBlock(block.id).id == block.id);

                    if(!blockExists)
                    {
                        if(blockchain->submitBlock(block))
                        {
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
                        response["data"].append(CryptoKernel::Blockchain::transactionToJson(tx));
                    }

                    response["nonce"] = request["nonce"];

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

                        returning["nonce"] = request["nonce"];

                        send(returning);
                    }
                    else
                    {
                        Json::Value response;
                        response["nonce"] = request["nonce"];
                        send(response);
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

                        Json::Value response;
                        response["data"] = CryptoKernel::Blockchain::blockToJson(currentBlock);

                        response["nonce"] = request["nonce"];

                        send(response);
                    }
                    else
                    {
                        Json::Value response;
                        response["data"] = CryptoKernel::Blockchain::blockToJson(blockchain->getBlock(request["data"]["id"].asString()));
                        response["nonce"] = request["nonce"];
                        send(response);
                    }
                }
            }
            else if(!request["nonce"].empty())
            {
                clientMutex.lock();
                responses[request["nonce"].asUInt64()] = request["data"];
                clientMutex.unlock();
            }
        }
        else
        {
            clientMutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
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

    send(request);
}

void CryptoKernel::Network::Peer::sendBlock(const CryptoKernel::Blockchain::block block)
{
    Json::Value request;
    request["command"] = "block";
    request["data"] = CryptoKernel::Blockchain::blockToJson(block);

    send(request);
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
    Json::Value blocks = sendRecv(request);

    std::vector<CryptoKernel::Blockchain::block> returning;
    for(unsigned int i = 0; i < blocks.size(); i++)
    {
        returning.push_back(CryptoKernel::Blockchain::jsonToBlock(blocks[i]));
    }

    return returning;
}
