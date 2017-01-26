#include "network.h"
#include "networkpeer.h"

CryptoKernel::Network::Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain)
{
    this->log = log;
    this->blockchain = blockchain;

    peers = new CryptoKernel::Storage("./peers");

    Json::Value seeds = peers->get("seeds");
    if(seeds.size() <= 0)
    {
        std::ifstream infile("peers.txt");
        if(!infile.is_open())
        {
            log->printf(LOG_LEVEL_ERR, "Network(): Could not open peers file");
        }

        std::string line;
        while(std::getline(infile, line))
        {
            Json::Value newSeed;
            newSeed["url"] = line;
            newSeed["lastseen"] = -1;
            newSeed["height"] = 1;
            seeds.append(newSeed);
        }

        infile.close();

        peers->store("seeds", seeds);
    }

    if(listener.listen(49000) != sf::Socket::Done)
    {
        log->printf(LOG_LEVEL_ERR, "Network(): Could not bind to port 49000");
    }

    running = true;

    // Start connection thread
    connectionThread.reset(new std::thread(&CryptoKernel::Network::connectionFunc, this));

    // Start management thread
    networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));
}

CryptoKernel::Network::~Network()
{
    running = false;
    connectionThread->join();
    networkThread->join();
    listener.close();
    delete peers;

    for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        delete it->second;
    }
}

void CryptoKernel::Network::networkFunc()
{
    while(running)
    {
        Json::Value seeds = peers->get("seeds");

        if(connected.size() < 8)
        {
            for(unsigned int i = 0; i < seeds.size(); i++)
            {
                if(connected.size() >= 8)
                {
                    break;
                }

                std::map<std::string, PeerInfo*>::iterator it = connected.find(seeds[i]["url"].asString());
                if(it != connected.end())
                {
                    continue;
                }

                log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + seeds[i]["url"].asString());

                // Attempt to connect to peer
                sf::TcpSocket* socket = new sf::TcpSocket();
                if(socket->connect(seeds[i]["url"].asString(), 49000, sf::seconds(3)) != sf::Socket::Done)
                {
                    log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + seeds[i]["url"].asString());
                    delete socket;
                    continue;
                }

                PeerInfo* peerInfo = new PeerInfo;
                peerInfo->peer.reset(new Peer(socket, blockchain, this));

                // Get height
                Json::Value info;
                try
                {
                    info = peerInfo->peer->getInfo();
                }
                catch(Peer::NetworkError& e)
                {
                    log->printf(LOG_LEVEL_WARN, "Network(): Error getting info from " + seeds[i]["url"].asString());
                    delete peerInfo;
                    continue;
                }

                log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + seeds[i]["url"].asString());

                // Update info
                seeds[i]["height"] = info["tipHeight"];

                std::time_t result = std::time(nullptr);
                seeds[i]["lastseen"] = std::asctime(std::localtime(&result));

                peerInfo->info = seeds[i];

                connected[seeds[i]["url"].asString()] = peerInfo;
            }
        }

        for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
        {
            try
            {
                const Json::Value info = it->second->peer->getInfo();
                it->second->info["height"] = info["tipHeight"];
                std::time_t result = std::time(nullptr);
                it->second->info["lastseen"] = std::asctime(std::localtime(&result));
            }
            catch(Peer::NetworkError& e)
            {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to contact " + it->first + ", disconnecting it");
                delete it->second;
                it = connected.erase(it);
            }
        }

        peers->store("seeds", seeds);

        //Determine best chain
        uint64_t currentHeight = blockchain->getBlock("tip").height;
        uint64_t bestHeight = currentHeight;
        for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
        {
            if(it->second->info["height"].asUInt64() > bestHeight)
            {
                bestHeight = it->second->info["height"].asUInt64();
            }
        }

        log->printf(LOG_LEVEL_INFO, "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " + std::to_string(bestHeight));

        //Detect if we are behind
        if(bestHeight > currentHeight)
        {
            for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
            {
                if(it->second->info["height"].asUInt64() > currentHeight)
                {
                    try
                    {
                        std::vector<CryptoKernel::Blockchain::block> blocks;
                        //Try to submit the first block
                        //If it fails get the 200 block previous until we can submit a block

                        do
                        {
                            log->printf(LOG_LEVEL_INFO, "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " + std::to_string(currentHeight + 201));
                            blocks = it->second->peer->getBlocks(currentHeight + 1, currentHeight + 201);
                            currentHeight = std::max(1, (int)currentHeight - 200);
                        } while(!blockchain->submitBlock(blocks[0]));


                        for(unsigned int i = 1; i < blocks.size(); i++)
                        {
                            if(!blockchain->submitBlock(blocks[i]))
                            {
                                break;
                            }
                        }
                        break;
                    }
                    catch(Peer::NetworkError& e)
                    {
                        log->printf(LOG_LEVEL_WARN, "Network(): Failed to contact " + it->first + " " + e.what());
                        continue;
                    }
                }
            }
        }

        //Rebroadcast unconfirmed transactions
        broadcastTransactions(blockchain->getUnconfirmedTransactions());

        if(bestHeight == currentHeight)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30000));
        }
    }
}

void CryptoKernel::Network::connectionFunc()
{
    while(running)
    {
        sf::TcpSocket* client = new sf::TcpSocket();
        if(listener.accept(*client) == sf::Socket::Done)
        {
            log->printf(LOG_LEVEL_INFO, "Network(): Peer connected from " + client->getRemoteAddress().toString() + ":" + std::to_string(client->getRemotePort()));
            PeerInfo* peerInfo = new PeerInfo();
            peerInfo->peer.reset(new Peer(client, blockchain, this));

            Json::Value info;

            try
            {
                info = peerInfo->peer->getInfo();
            }
            catch(Peer::NetworkError& e)
            {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to get information from connecting peer");
                delete peerInfo;
                continue;
            }

            peerInfo->info["height"] = info["tipHeight"];

            std::time_t result = std::time(nullptr);
            peerInfo->info["lastseen"] = std::asctime(std::localtime(&result));

            connected[client->getRemoteAddress().toString()] = peerInfo;
        }
        else
        {
            log->printf(LOG_LEVEL_WARN, "Network(): Failed to accept incoming connection");
            delete client;
        }
    }
}

unsigned int CryptoKernel::Network::getConnections()
{
    return connected.size();
}

void CryptoKernel::Network::broadcastTransactions(const std::vector<CryptoKernel::Blockchain::transaction> transactions)
{
    for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        it->second->peer->sendTransactions(transactions);
    }
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block)
{
    for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        it->second->peer->sendBlock(block);
    }
}

double CryptoKernel::Network::syncProgress()
{
    uint64_t currentHeight = blockchain->getBlock("tip").height;
    uint64_t bestHeight = currentHeight;
    for(std::map<std::string, PeerInfo*>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        if(it->second->info["height"].asUInt64() > bestHeight)
        {
            bestHeight = it->second->info["height"].asUInt64();
        }
    }

    return (double)(currentHeight)/(double)(bestHeight);
}
