/*  CryptoKernel - A library for creating blockchain based digital currency
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

#include "version.h"
#include "ckmath.h"
#include "network.h"
#include "chainsync.h"

CryptoKernel::Network::Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain)
{
    this->log = log;
    this->blockchain = blockchain;

    listener.reset(new sf::TcpListener());

    if(listener->listen(49000) != sf::Socket::Done)
    {
        log->printf(LOG_LEVEL_ERR, "Network(): Failed to start server");
    }

    running = true;
    listener->setBlocking(false);

    std::ifstream peersfile("peers.txt");
    if(!peersfile.is_open())
    {
        log->printf(LOG_LEVEL_ERR, "Network(): Could not open peers list");
    }

    std::string line;
    while(std::getline(peersfile, line))
    {
        ips.push_back(line);
    }
    peersfile.close();

    connectionsThread.reset(new std::thread(&CryptoKernel::Network::handleConnections, this));

    chainSync.reset(new ChainSync(blockchain, this));

    checkRep();
}

CryptoKernel::Network::~Network()
{
    running = false;
    connectionsThread->join();
}

unsigned int CryptoKernel::Network::getConnections()
{
    return peers.size();
}

void CryptoKernel::Network::sendBlock(const CryptoKernel::Blockchain::block block)
{
    for(Peer* peer : peers)
    {
        peer->sendBlock(block);
    }
}

void CryptoKernel::Network::sendTransaction(const CryptoKernel::Blockchain::transaction tx)
{
    for(Peer* peer : peers)
    {
        peer->sendTransaction(tx);
    }
}

CryptoKernel::Blockchain::block CryptoKernel::Network::getBlock(const std::string id)
{
    std::uniform_int_distribution<unsigned int> distribution(0, peers.size() - 1);
    for(unsigned int i = 0; i < peers.size(); i++)
    {
        const unsigned int peerId = distribution(generator);
        if(peers[peerId]->isConnected())
        {
            const CryptoKernel::Blockchain::block returning = peers[peerId]->getBlock(id);
            if(returning.id == id)
            {
                return returning;
            }
        }
    }

    throw NotFoundException();
}

CryptoKernel::Blockchain::block CryptoKernel::Network::getBlockByHeight(const uint64_t height)
{
    std::uniform_int_distribution<unsigned int> distribution(0, peers.size() - 1);
    for(unsigned int i = 0; i < peers.size(); i++)
    {
        const unsigned int peerId = distribution(generator);
        if(peers[peerId]->isConnected() && nodes[peers[peerId]->getAddress()] >= height)
        {
            const CryptoKernel::Blockchain::block returning = peers[peerId]->getBlockByHeight(height);
            if(returning.height == height)
            {
                return returning;
            }
        }
    }

    throw NotFoundException();
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::getBlocks(const std::string id)
{
    std::uniform_int_distribution<unsigned int> distribution(0, peers.size() - 1);
    for(unsigned int i = 0; i < peers.size(); i++)
    {
        const unsigned int peerId = distribution(generator);
        if(peers[peerId]->isConnected())
        {
            return peers[peerId]->getBlocks(id);
        }
    }

    throw NotFoundException();
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::getBlocksByHeight(const uint64_t height)
{
    std::uniform_int_distribution<unsigned int> distribution(0, peers.size() - 1);
    for(unsigned int i = 0; i < peers.size(); i++)
    {
        const unsigned int peerId = distribution(generator);
        if(peers[peerId]->isConnected() && nodes[peers[peerId]->getAddress()] >= height)
        {
            return peers[peerId]->getBlocksByHeight(height);
        }
    }

    throw NotFoundException();
}

bool CryptoKernel::Network::connectPeer(const std::string peerAddress)
{
    sf::TcpSocket* client = new sf::TcpSocket();
    log->printf(LOG_LEVEL_INFO, "Network::handleConnections(): Attempting to connect to " + peerAddress);
    if(client->connect(peerAddress, 49000, sf::seconds(5)) == sf::Socket::Done)
    {
        Peer* peer = new Peer(client, blockchain);
        peers.push_back(peer);
        log->printf(LOG_LEVEL_INFO, "Network::handleConnections(): Successfully connected to " + peerAddress);
        nodes.insert(std::pair<std::string, uint64_t>(peer->getAddress(), 0));
        return true;
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "Network::handleConnections(): Failed to connect to " + peerAddress);
        return false;
    }
}

void CryptoKernel::Network::handleConnections()
{
    unsigned int currentNode = 0;
    while(running)
    {
        if(getConnections() < 8 && ips.size() > 0 && nodes.find(ips[currentNode]) == nodes.end())
        {
            if(!connectPeer(ips[currentNode]))
            {
                ips.erase(ips.begin() + currentNode);
            }

            currentNode++;
            if(currentNode >= nodes.size())
            {
                currentNode = 0;
            }
        }

        sf::TcpSocket* client = new sf::TcpSocket();
        sf::Socket::Status status;
        if((status = listener->accept(*client)) == sf::Socket::Error)
        {
            log->printf(LOG_LEVEL_ERR, "Network::handleConnections(): Failed to accept incoming connection");
        }
        else if(status == sf::Socket::Done && nodes.find(client->getRemoteAddress().toString()) == nodes.end())
        {
            Peer* peer = new Peer(client, blockchain);
            peers.push_back(peer);
            nodes.insert(std::pair<std::string, uint64_t>(peer->getAddress(), 0));
            log->printf(LOG_LEVEL_INFO, "Network::handleConnections(): Successfully connected to " + peer->getAddress());
        }

        std::vector<Peer*>::iterator it;
        for(it = peers.begin(); it < peers.end(); it++)
        {
            if(!(*it)->isConnected())
            {
                log->printf(LOG_LEVEL_INFO, "wowe we goofed");
                nodes.erase((*it)->getAddress());
                delete (*it);
                it = peers.erase(it);
            }
            else
            {
                const Json::Value peerInfo = (*it)->getInfo();
                const CryptoKernel::Blockchain::block peerTip = CryptoKernel::Blockchain::jsonToBlock(peerInfo["tipBlock"]);
                nodes[(*it)->getAddress()] = peerTip.height;
            }
        }
    }
}

void CryptoKernel::Network::checkRep()
{
    assert(blockchain != nullptr);
    assert(log != nullptr);
    assert(listener.get() != nullptr);
    assert(chainSync.get() != nullptr);
}

CryptoKernel::Network::Peer::Peer(sf::TcpSocket* socket, CryptoKernel::Blockchain* blockchain)
{
    connected = true;
    this->socket.reset(socket);
    this->blockchain = blockchain;

    Json::Value request;
    request["command"] = "info";
    Json::Value data;
    data["version"] = version;
    data["tipBlock"] = CryptoKernel::Blockchain::blockToJson(blockchain->getBlock("tip"));
    request["data"] = data;
    const Json::Value info = sendRecv(request);

    const std::string peerVersion = info["data"]["version"].asString();
    if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find(".")))
    {
        disconnect();
    }
    else
    {
        eventThread.reset(new std::thread(&CryptoKernel::Network::Peer::handleEvents, this));
    }
}

CryptoKernel::Network::Peer::~Peer()
{
    disconnect();
    eventThread->join();
}

Json::Value CryptoKernel::Network::Peer::sendRecv(const Json::Value data)
{
    const std::string packetData = CryptoKernel::Storage::toString(data, false);
    sf::Packet packet;
    packet.append(packetData.c_str(), packetData.size());
    peerLock.lock();
    sf::Socket::Status status;
    socket->setBlocking(true);
    if((status = socket->send(packet)) == sf::Socket::Done)
    {
        packet.clear();
        if((status = socket->receive(packet)) == sf::Socket::Done)
        {
            const std::string receivedPacket((char*)packet.getData(), packet.getDataSize());
            const Json::Value infoPacket = CryptoKernel::Storage::toJson(receivedPacket);
            peerLock.unlock();
            return infoPacket;
        }
    }

    if(status == sf::Socket::Error || status == sf::Socket::Disconnected)
    {
        disconnect();
    }

    peerLock.unlock();
    return Json::Value();
}

bool CryptoKernel::Network::Peer::isConnected()
{
    return connected;
}

void CryptoKernel::Network::Peer::handleEvents()
{
    while(connected)
    {
        peerLock.lock();
        socket->setBlocking(false);
        sf::Socket::Status status;
        sf::Packet packet;
        if((status = socket->receive(packet)) == sf::Socket::Done)
        {
            const std::string receivedPacket((char*)packet.getData(), packet.getDataSize());
            const Json::Value jsonPacket = CryptoKernel::Storage::toJson(receivedPacket);
            socket->setBlocking(true);
            Json::Value request;
            if(jsonPacket["command"].asString() == "sendinfo")
            {
                request["command"] = "info";

                Json::Value data;
                data["version"] = version;
                data["tipBlock"] = CryptoKernel::Blockchain::blockToJson(blockchain->getBlock("tip"));
                request["data"] = data;
                break;
            }
            else if(jsonPacket["command"].asString() == "getblock")
            {
                request["command"] = "block";
                if(jsonPacket["height"].empty())
                {
                    request["data"] = CryptoKernel::Blockchain::blockToJson(blockchain->getBlock(request["id"].asString()));
                }
                else
                {
                    request["data"] = CryptoKernel::Blockchain::blockToJson(blockchain->getBlockByHeight(request["height"].asUInt64()));
                }
                break;
            }
            else if(jsonPacket["command"].asString() == "getblocks")
            {
                request["command"] = "blocks";
                if(jsonPacket["height"].empty())
                {
                    CryptoKernel::Blockchain::block Block = blockchain->getBlock(request["id"].asString());
                    bool appended = true;
                    for(unsigned int i = 0; i < 500 && appended; i++)
                    {
                        appended = false;
                        CryptoKernel::Storage::Iterator* it = blockchain->newIterator();
                        for(it->SeekToFirst(); it->Valid(); it->Next())
                        {
                            if(it->value()["previousBlockId"].asString() == Block.id && Block.id != "" && it->value()["mainChain"].asBool())
                            {
                                appended = true;
                                Block.id = it->value()["id"].asString();
                                request["data"].append(it->value());
                                break;
                            }
                        }
                        delete it;
                    }
                }
                else
                {
                    CryptoKernel::Blockchain::block block = blockchain->getBlockByHeight(jsonPacket["height"].asUInt64() + 500);
                    std::vector<CryptoKernel::Blockchain::block> blocks;
                    for(unsigned int i = 0; i < 500; i++)
                    {
                        if(block.height == jsonPacket["height"].asUInt64() + 500 - i && block.id != "")
                        {
                            blocks.insert(blocks.begin(), block);
                            block = blockchain->getBlock(block.previousBlockId);
                        }
                        else
                        {
                            break;
                        }
                    }

                    for(const CryptoKernel::Blockchain::block thisBlock : blocks)
                    {
                        request["data"].append(CryptoKernel::Blockchain::blockToJson(thisBlock));
                    }
                }
            }
            else if(jsonPacket["command"].asString() == "block")
            {
                const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::jsonToBlock(jsonPacket["data"]);
                blockchain->submitBlock(block);
            }
            else if(jsonPacket["command"].asString() == "transaction")
            {
                const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::jsonToTransaction(jsonPacket["data"]);
                blockchain->submitTransaction(tx);
            }
            else
            {
                disconnect();
            }

            status = socket->send(packet);
        }
        else if(status == sf::Socket::Error || status == sf::Socket::Disconnected)
        {
            disconnect();
        }
        socket->setBlocking(true);
        peerLock.unlock();
    }
}

void CryptoKernel::Network::Peer::disconnect()
{
    connected = false;
    socket->disconnect();
}

void CryptoKernel::Network::Peer::send(const Json::Value data)
{
    const std::string packetData = CryptoKernel::Storage::toString(data, false);
    sf::Packet packet;
    packet.append(packetData.c_str(), packetData.size());
    peerLock.lock();
    socket->setBlocking(false);
    socket->send(packet);
    socket->setBlocking(true);
    peerLock.unlock();
}

void CryptoKernel::Network::Peer::sendBlock(const CryptoKernel::Blockchain::block block)
{
    Json::Value request;
    request["command"] = "block";
    request["data"] = CryptoKernel::Blockchain::blockToJson(block);
    send(request);
}

void CryptoKernel::Network::Peer::sendTransaction(const CryptoKernel::Blockchain::transaction tx)
{
    Json::Value request;
    request["command"] = "transaction";
    request["data"] = CryptoKernel::Blockchain::transactionToJson(tx);
    send(request);
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Peer::getBlock(const std::string id)
{
    Json::Value request;
    request["command"] = "getblock";
    request["id"] = id;
    const Json::Value response = sendRecv(request);

    if(response["command"].asString() != "block")
    {
        disconnect();
        return CryptoKernel::Blockchain::block();
    }
    else
    {
        return CryptoKernel::Blockchain::jsonToBlock(request["data"]);
    }
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Peer::getBlockByHeight(const uint64_t height)
{
    Json::Value request;
    request["command"] = "getblock";
    request["height"] = static_cast<unsigned long long int>(height);
    const Json::Value response = sendRecv(request);

    if(response["command"].asString() != "block")
    {
        disconnect();
        return CryptoKernel::Blockchain::block();
    }
    else
    {
        return CryptoKernel::Blockchain::jsonToBlock(request["data"]);
    }
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Peer::getBlocks(const std::string id)
{
    Json::Value request;
    request["command"] = "getblocks";
    request["id"] = id;
    const Json::Value response = sendRecv(request);

    if(response["command"].asString() != "blocks")
    {
        disconnect();
        return std::vector<CryptoKernel::Blockchain::block>();
    }
    else
    {
        std::vector<CryptoKernel::Blockchain::block> returning;
        for(unsigned int i = 0; i < response["data"].size(); i++)
        {
            returning.push_back(CryptoKernel::Blockchain::jsonToBlock(response["data"][i]));
        }

        return returning;
    }
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Peer::getBlocksByHeight(const uint64_t height)
{
    Json::Value request;
    request["command"] = "getblocks";
    request["height"] = static_cast<unsigned long long int>(height);
    const Json::Value response = sendRecv(request);

    if(response["command"].asString() != "blocks")
    {
        disconnect();
        return std::vector<CryptoKernel::Blockchain::block>();
    }
    else
    {
        std::vector<CryptoKernel::Blockchain::block> returning;
        for(unsigned int i = 0; i < response["data"].size(); i++)
        {
            returning.push_back(CryptoKernel::Blockchain::jsonToBlock(response["data"][i]));
        }

        return returning;
    }
}

Json::Value CryptoKernel::Network::Peer::getInfo()
{
    Json::Value request;
    request["command"] = "sendinfo";

    const Json::Value infoPacket = sendRecv(request);
    if(infoPacket["command"].asString() != "info")
    {
        disconnect();
    }

    return infoPacket;
}

std::string CryptoKernel::Network::Peer::getAddress()
{
    return socket->getRemoteAddress().toString();
}
