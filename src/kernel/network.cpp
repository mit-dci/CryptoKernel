#include "network.h"
#include "networkpeer.h"
#include "version.h"

CryptoKernel::Network::Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain)
{
    this->log = log;
    this->blockchain = blockchain;

    networkdb.reset(new CryptoKernel::Storage("./peers"));
    peers.reset(new Storage::Table("peers"));

    std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

    std::ifstream infile("peers.txt");
    if(!infile.is_open())
    {
        log->printf(LOG_LEVEL_ERR, "Network(): Could not open peers file");
    }

    std::string line;
    while(std::getline(infile, line))
    {
        if(!peers->get(dbTx.get(), line).isObject()) {
            Json::Value newSeed;
            newSeed["lastseen"] = -1;
            newSeed["height"] = 1;
            newSeed["score"] = 0;
            peers->put(dbTx.get(), line, newSeed);
        }
    }

    infile.close();

    dbTx->commit();

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
}

void CryptoKernel::Network::networkFunc()
{
    while(running)
    {
        CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(peers.get(), networkdb.get());

        for(it->SeekToFirst(); it->Valid(); it->Next())
        {
            if(connected.size() >= 8)
            {
                break;
            }

            Json::Value peer = it->value();

            if(connected.find(it->key()) != connected.end())
            {
                continue;
            }

            if(std::find(banned.begin(), banned.end(), it->key()) != banned.end()) {
                continue;
            }

            log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + it->key());

            // Attempt to connect to peer
            sf::TcpSocket* socket = new sf::TcpSocket();
            if(socket->connect(it->key(), 49000, sf::seconds(3)) != sf::Socket::Done)
            {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + it->key());
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
                log->printf(LOG_LEVEL_WARN, "Network(): Error getting info from " + it->key());
                delete peerInfo;
                continue;
            }

            log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + it->key());

            // Update info
            try {
                peer["height"] = info["tipHeight"].asUInt64();
            } catch(const Json::Exception& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): " + it->key() + " sent a malformed info message");
                delete peerInfo;
                continue;
            }

            std::time_t result = std::time(nullptr);
            peer["lastseen"] = std::asctime(std::localtime(&result));

            peer["score"] = 0;

            peerInfo->info = peer;

            connected[it->key()].reset(peerInfo);
        }

        delete it;

        for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
        {
            try
            {
                const Json::Value info = it->second->peer->getInfo();
                try {
                    const std::string peerVersion = info["version"].asString();
                    if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find(".")))
                    {
                        log->printf(LOG_LEVEL_WARN, "Network(): " + it->first + " has a different major version than us");
                        throw Peer::NetworkError();
                    }

                    it->second->info["height"] = info["tipHeight"].asUInt64();
                } catch(const Json::Exception& e) {
                    log->printf(LOG_LEVEL_WARN, "Network(): " + it->first + " sent a malformed info message");
                    throw Peer::NetworkError();
                }

                const std::time_t result = std::time(nullptr);
                it->second->info["lastseen"] = std::asctime(std::localtime(&result));
            }
            catch(const Peer::NetworkError& e)
            {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to contact " + it->first + ", disconnecting it");
                it = connected.erase(it);
                if(connected.size() < 1)
                {
                    break;
                }
            }
        }

        //Determine best chain
        uint64_t currentHeight = blockchain->getBlockDB("tip").getHeight();
        uint64_t bestHeight = currentHeight;
        for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
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
            for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
            {
                if(it->second->info["height"].asUInt64() > currentHeight)
                {
                    std::vector<CryptoKernel::Blockchain::block> blocks;
                    //Try to submit the first block
                    //If it fails get the 200 block previous until we can submit a block

                    do
                    {
                        log->printf(LOG_LEVEL_INFO, "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " + std::to_string(currentHeight + 201));
                        try {
                            blocks = it->second->peer->getBlocks(currentHeight + 1, currentHeight + 201);
                        } catch(Peer::NetworkError& e) {
                            log->printf(LOG_LEVEL_WARN, "Network(): Failed to contact " + it->first + " " + e.what() + " while downloading blocks");
                            break;
                        }
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
            }
        }

        //Rebroadcast unconfirmed transactions
        //broadcastTransactions(blockchain->getUnconfirmedTransactions());

        if(bestHeight == currentHeight)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20000));
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
            if(connected.find(client->getRemoteAddress().toString()) != connected.end()) {
                log->printf(LOG_LEVEL_INFO, "Network(): Incoming connection duplicates existing connection for " + client->getRemoteAddress().toString());
                client->disconnect();
                delete client;
                continue;
            }

            if(std::find(banned.begin(), banned.end(), client->getRemoteAddress().toString()) != banned.end()) {
                log->printf(LOG_LEVEL_INFO, "Network(): Incoming connection " + client->getRemoteAddress().toString() + " is banned");
                client->disconnect();
                delete client;
                continue;
            }

            log->printf(LOG_LEVEL_INFO, "Network(): Peer connected from " + client->getRemoteAddress().toString() + ":" + std::to_string(client->getRemotePort()));
            PeerInfo* peerInfo = new PeerInfo();
            peerInfo->peer.reset(new Peer(client, blockchain, this));

            Json::Value info;

            try
            {
                info = peerInfo->peer->getInfo();
            }
            catch(const Peer::NetworkError& e)
            {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to get information from connecting peer");
                delete peerInfo;
                continue;
            }

            try {
                peerInfo->info["height"] = info["tipHeight"].asUInt64();
            } catch(const Json::Exception& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): Incoming peer sent invalid info message");
                delete peerInfo;
                continue;
            }

            const std::time_t result = std::time(nullptr);
            peerInfo->info["lastseen"] = std::asctime(std::localtime(&result));

            peerInfo->info["score"] = 0;

            connected[client->getRemoteAddress().toString()].reset(peerInfo);
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
    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        try {
            it->second->peer->sendTransactions(transactions);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastTransactions(): Failed to contact peer");
        }
    }
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block)
{
    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        try {
            it->second->peer->sendBlock(block);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer");
        }
    }
}

double CryptoKernel::Network::syncProgress()
{
    uint64_t currentHeight = blockchain->getBlockDB("tip").getHeight();
    uint64_t bestHeight = currentHeight;
    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin(); it != connected.end(); it++)
    {
        if(it->second->info["height"].asUInt64() > bestHeight)
        {
            bestHeight = it->second->info["height"].asUInt64();
        }
    }

    return (double)(currentHeight)/(double)(bestHeight);
}

void CryptoKernel::Network::changeScore(const std::string& url, const uint64_t score) {
    connected[url]->info["score"] = connected[url]->info["score"].asUInt64() + score;
    if(connected[url]->info["score"].asUInt64() > 200) {
        connected.erase(url);
        banned.push_back(url);
    }
}
