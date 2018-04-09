#include "network.h"
#include "networkpeer.h"
#include "version.h"

#include <list>

CryptoKernel::Network::Network(CryptoKernel::Log* log,
                               CryptoKernel::Blockchain* blockchain,
                               const unsigned int port,
                               const std::string& dbDir) {
    this->log = log;
    this->blockchain = blockchain;
    this->port = port;
    bestHeight = 0;

    myAddress = sf::IpAddress::getPublicAddress();

    networkdb.reset(new CryptoKernel::Storage(dbDir));
    peers.reset(new Storage::Table("peers"));

    std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

    std::ifstream infile("peers.txt");
    if(!infile.is_open()) {
        log->printf(LOG_LEVEL_ERR, "Network(): Could not open peers file");
    }

    std::string line;
    while(std::getline(infile, line)) {
        if(!peers->get(dbTx.get(), line).isObject()) {
            Json::Value newSeed;
            newSeed["lastseen"] = 0;
            newSeed["height"] = 1;
            newSeed["score"] = 0;
            peers->put(dbTx.get(), line, newSeed);
        }
    }

    infile.close();

    dbTx->commit();

    if(listener.listen(port) != sf::Socket::Done) {
        log->printf(LOG_LEVEL_ERR, "Network(): Could not bind to port " + std::to_string(port));
    }

    running = true;

    listener.setBlocking(false);

    // Start connection thread
    connectionThread.reset(new std::thread(&CryptoKernel::Network::connectionFunc, this));

    // Start management thread
    networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));

    // Start peer thread
    peerThread.reset(new std::thread(&CryptoKernel::Network::peerFunc, this));
}

CryptoKernel::Network::~Network() {
    running = false;
    connectionThread->join();
    networkThread->join();
    peerThread->join();
    listener.close();
}

void CryptoKernel::Network::peerFunc() {
    while(running) {
        bool wait = false;

        std::map<std::string, Json::Value> peerInfos;

        {
            std::lock_guard<std::recursive_mutex> lock(connectedMutex);
            CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(
                peers.get(), networkdb.get());

            for(it->SeekToFirst(); it->Valid(); it->Next()) {
                if(connected.size() >= 8) {
                    wait = true;
                    break;
                }

                Json::Value peer = it->value();

                if(connected.find(it->key()) != connected.end()) {
                    continue;
                }

                std::time_t result = std::time(nullptr);

                const auto banIt = banned.find(it->key());
                if(banIt != banned.end()) {
                    if(banIt->second > static_cast<uint64_t>(result)) {
                        continue;
                    }
                }

                if(peer["lastattempt"].asUInt64() + 5 * 60 > static_cast<unsigned long long int>
                        (result) && peer["lastattempt"].asUInt64() != peer["lastseen"].asUInt64()) {
                    continue;
                }

                sf::IpAddress addr(it->key());

                if(addr == sf::IpAddress::getLocalAddress()
                        || addr == myAddress
                        || addr == sf::IpAddress::LocalHost
                        || addr == sf::IpAddress::None) {
                    continue;
                }

                log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + it->key());

                peer["lastattempt"] = static_cast<uint64_t>(result);

                // Attempt to connect to peer
                sf::TcpSocket* socket = new sf::TcpSocket();
                if(socket->connect(it->key(), port, sf::seconds(3)) != sf::Socket::Done) {
                    log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + it->key());
                    delete socket;
                    peerInfos[it->key()] = peer;
                    break;
                }

                PeerInfo* peerInfo = new PeerInfo;
                peerInfo->peer.reset(new Peer(socket, blockchain, this, false));

                // Get height
                Json::Value info;
                try {
                    info = peerInfo->peer->getInfo();
                } catch(Peer::NetworkError& e) {
                    log->printf(LOG_LEVEL_WARN, "Network(): Error getting info from " + it->key());
                    delete peerInfo;
                    peerInfos[it->key()] = peer;
                    break;
                }

                log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + it->key());

                // Update info
                try {
                    peer["height"] = info["tipHeight"].asUInt64();
                    peer["version"] = info["version"].asString();
                } catch(const Json::Exception& e) {
                    log->printf(LOG_LEVEL_WARN, "Network(): " + it->key() + " sent a malformed info message");
                    delete peerInfo;
                    peerInfos[it->key()] = peer;
                    break;
                }

                peer["lastseen"] = static_cast<uint64_t>(result);

                peer["score"] = 0;

                peerInfo->info = peer;

                connected[it->key()].reset(peerInfo);
                peerInfos[it->key()] = peer;
                break;
            }

            if(!it->Valid()) {
                wait = true;
            }

            delete it;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(connectedMutex);
            std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

	    std::set<std::string> removals;

	    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
                    it != connected.end(); it++) {
                try {
                    const Json::Value info = it->second->peer->getInfo();
                    try {
                        const std::string peerVersion = info["version"].asString();
                        if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find("."))) {
                            log->printf(LOG_LEVEL_WARN,
                                        "Network(): " + it->first + " has a different major version than us");
                            throw Peer::NetworkError();
                        }

                        it->second->info["height"] = info["tipHeight"].asUInt64();

                        for(const Json::Value& peer : info["peers"]) {
                            sf::IpAddress addr(peer.asString());
                            if(addr != sf::IpAddress::None) {
                                if(!peers->get(dbTx.get(), addr.toString()).isObject()) {
                                    log->printf(LOG_LEVEL_INFO, "Network(): Discovered new peer: " + addr.toString());
                                    Json::Value newSeed;
                                    newSeed["lastseen"] = 0;
                                    newSeed["height"] = 1;
                                    newSeed["score"] = 0;
                                    peers->put(dbTx.get(), addr.toString(), newSeed);
                                }
                            } else {
                                changeScore(it->first, 10);
                                throw Peer::NetworkError();
                            }
                        }
                    } catch(const Json::Exception& e) {
                        changeScore(it->first, 50);
                        throw Peer::NetworkError();
                    }

                    const std::time_t result = std::time(nullptr);
                    it->second->info["lastseen"] = static_cast<uint64_t>(result);
                } catch(const Peer::NetworkError& e) {
                    log->printf(LOG_LEVEL_WARN,
                                "Network(): Failed to contact " + it->first + ", disconnecting it");
                    removals.insert(it->first);
                }
            }

	    for(const auto& peer : removals) {
            const auto it = connected.find(peer);
            if(it != connected.end()) {
                connected.erase(it);
            }
	    }

            for(const auto& peer : peerInfos) {
                peers->put(dbTx.get(), peer.first, peer.second);
            }

            dbTx->commit();
        }

        if(wait) {
            std::this_thread::sleep_for(std::chrono::seconds(20));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void CryptoKernel::Network::networkFunc() {
    std::unique_ptr<std::thread> blockProcessor;
    bool failure = false;
    uint64_t currentHeight = blockchain->getBlockDB("tip").getHeight();
    this->currentHeight = currentHeight;
    uint64_t startHeight = currentHeight;

    while(running) {
        //Determine best chain
        connectedMutex.lock();
        uint64_t bestHeight = currentHeight;
        for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
                it != connected.end(); it++) {
            if(it->second->info["height"].asUInt64() > bestHeight) {
                bestHeight = it->second->info["height"].asUInt64();
            }
            const auto banIt = banned.find(it->first);
            if(banIt != banned.end()) {
                if(banIt->second > static_cast<uint64_t>(std::time(nullptr))) {
                    log->printf(LOG_LEVEL_WARN,
                                "Network(): Disconnecting " + it->first + " for being banned");
                    it = connected.erase(it);
                    if(connected.size() < 1) {
                        break;
                    }
                }
            }
        }

        if(this->currentHeight > bestHeight) {
            bestHeight = this->currentHeight;
        }
        this->bestHeight = bestHeight;
        connectedMutex.unlock();

        log->printf(LOG_LEVEL_INFO,
                    "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " +
                    std::to_string(bestHeight));

        bool madeProgress = false;

        //Detect if we are behind
        if(bestHeight > currentHeight) {
            connectedMutex.lock();

            for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
                    it != connected.end() && running; ) {
                if(it->second->info["height"].asUInt64() > currentHeight) {
                    std::list<CryptoKernel::Blockchain::block> blocks;

                    if(currentHeight == startHeight) {
                        auto nBlocks = 0;
                        do {
                            log->printf(LOG_LEVEL_INFO,
                                        "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " +
                                        std::to_string(currentHeight + 6));
                            try {
                                const auto newBlocks = it->second->peer->getBlocks(currentHeight + 1, currentHeight + 6);
                                nBlocks = newBlocks.size();
                                blocks.insert(blocks.end(), newBlocks.rbegin(), newBlocks.rend());
                                if(nBlocks > 0) {
                                    madeProgress = true;
                                }
                            } catch(Peer::NetworkError& e) {
                                log->printf(LOG_LEVEL_WARN,
                                            "Network(): Failed to contact " + it->first + " " + e.what() +
                                            " while downloading blocks");
                                it++;
                                break;
                            }

                            try {
                                blockchain->getBlockDB(blocks.rbegin()->getPreviousBlockId().toString());
                            } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                if(currentHeight == 1) {
                                    // This peer has a different genesis block to us
                                    changeScore(it->first, 250);
                                    it++;
                                    break;
                                } else {
                                    currentHeight = std::max(1, (int)currentHeight - nBlocks);
                                    continue;
                                }
                            }

                            break;
                        } while(running);

                        currentHeight += nBlocks;
                    }

                    while(blocks.size() < 2000 && running && currentHeight < bestHeight) {
                        log->printf(LOG_LEVEL_INFO,
                                    "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " +
                                    std::to_string(currentHeight + 6));

                        auto nBlocks = 0;
                        try {
                            const auto newBlocks = it->second->peer->getBlocks(currentHeight + 1, currentHeight + 6);
                            nBlocks = newBlocks.size();
                            blocks.insert(blocks.begin(), newBlocks.rbegin(), newBlocks.rend());
                            if(nBlocks > 0) {
                                madeProgress = true;
                            }
                        } catch(Peer::NetworkError& e) {
                            log->printf(LOG_LEVEL_WARN,
                                        "Network(): Failed to contact " + it->first + " " + e.what() +
                                        " while downloading blocks");
                            it++;
                            break;
                        }

                        currentHeight = std::min(currentHeight + std::max(nBlocks, 1), bestHeight);
                    }

                    if(blockProcessor) {
                        blockProcessor->join();
                        blockProcessor.reset();

                        if(failure) {
                            blocks.clear();
                            currentHeight = blockchain->getBlockDB("tip").getHeight();
                            this->currentHeight = currentHeight;
                            startHeight = currentHeight;
                            bestHeight = currentHeight;
                            it++;
                            break;
                        }

                        if(currentHeight == bestHeight) {
                            break;
                        }
                    }

                    blockProcessor.reset(new std::thread([&, blocks](const std::string& peer){
                        failure = false;

                        for(auto rit = blocks.rbegin(); rit != blocks.rend(); ++rit) {
                            const auto blockResult = blockchain->submitBlock(*rit);

                            if(std::get<1>(blockResult)) {
                                changeScore(peer, 50);
                            }

                            if(!std::get<0>(blockResult)) {
                                failure = true;
                                break;
                            }
                        }
                    }, it->first));
                } else {
                    it++;
                }
            }

            connectedMutex.unlock();
        }

        if(bestHeight <= currentHeight || connected.size() == 0 || !madeProgress) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20000));
            currentHeight = blockchain->getBlockDB("tip").getHeight();
            startHeight = currentHeight;
            this->currentHeight = currentHeight;
        }
    }

    if(blockProcessor) {
        blockProcessor->join();
    }
}

void CryptoKernel::Network::connectionFunc() {
    while(running) {
        sf::TcpSocket* client = new sf::TcpSocket();
        if(listener.accept(*client) == sf::Socket::Done) {
            std::lock_guard<std::recursive_mutex> lock(connectedMutex);
            if(connected.find(client->getRemoteAddress().toString()) != connected.end()) {
                log->printf(LOG_LEVEL_INFO,
                            "Network(): Incoming connection duplicates existing connection for " +
                            client->getRemoteAddress().toString());
                client->disconnect();
                delete client;
                continue;
            }

            const auto it = banned.find(client->getRemoteAddress().toString());
            if(it != banned.end()) {
                if(it->second > static_cast<uint64_t>(std::time(nullptr))) {
                    log->printf(LOG_LEVEL_INFO,
                                "Network(): Incoming connection " + client->getRemoteAddress().toString() + " is banned");
                    client->disconnect();
                    delete client;
                    continue;
                }
            }

            sf::IpAddress addr(client->getRemoteAddress());

            if(addr == sf::IpAddress::getLocalAddress()
                    || addr == myAddress
                    || addr == sf::IpAddress::LocalHost
                    || addr == sf::IpAddress::None) {
                log->printf(LOG_LEVEL_INFO,
                            "Network(): Incoming connection " + client->getRemoteAddress().toString() +
                            " is connecting to self");
                client->disconnect();
                delete client;
                continue;
            }


            log->printf(LOG_LEVEL_INFO,
                        "Network(): Peer connected from " + client->getRemoteAddress().toString() + ":" +
                        std::to_string(client->getRemotePort()));
            PeerInfo* peerInfo = new PeerInfo();
            peerInfo->peer.reset(new Peer(client, blockchain, this, true));

            Json::Value info;

            try {
                info = peerInfo->peer->getInfo();
            } catch(const Peer::NetworkError& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to get information from connecting peer");
                delete peerInfo;
                continue;
            }

            try {
                peerInfo->info["height"] = info["tipHeight"].asUInt64();
                peerInfo->info["version"] = info["version"].asString();
            } catch(const Json::Exception& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): Incoming peer sent invalid info message");
                delete peerInfo;
                continue;
            }

            const std::time_t result = std::time(nullptr);
            peerInfo->info["lastseen"] = static_cast<uint64_t>(result);

            peerInfo->info["score"] = 0;

            connected[client->getRemoteAddress().toString()].reset(peerInfo);

            std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());
            peers->put(dbTx.get(), client->getRemoteAddress().toString(), peerInfo->info);
            dbTx->commit();
        } else {
            delete client;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

unsigned int CryptoKernel::Network::getConnections() {
    return connected.size();
}

void CryptoKernel::Network::broadcastTransactions(const
        std::vector<CryptoKernel::Blockchain::transaction> transactions) {
    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
            it != connected.end(); it++) {
        try {
            it->second->peer->sendTransactions(transactions);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastTransactions(): Failed to contact peer");
        }
    }
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block) {
    for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
            it != connected.end(); it++) {
        try {
            it->second->peer->sendBlock(block);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer");
        }
    }
}

double CryptoKernel::Network::syncProgress() {
    return (double)(currentHeight)/(double)(bestHeight);
}

void CryptoKernel::Network::changeScore(const std::string& url, const uint64_t score) {
    connected[url]->info["score"] = connected[url]->info["score"].asUInt64() + score;
    log->printf(LOG_LEVEL_WARN,
                "Network(): " + url + " misbehaving, increasing ban score by " + std::to_string(
                    score) + " to " + connected[url]->info["score"].asString());
    if(connected[url]->info["score"].asUInt64() > 200) {
        log->printf(LOG_LEVEL_WARN,
                    "Network(): Banning " + url + " for being above the ban score threshold");
        // Ban for 24 hours
        banned[url] = static_cast<uint64_t>(std::time(nullptr)) + 24 * 60 * 60;
    }
}

std::set<std::string> CryptoKernel::Network::getConnectedPeers() {
    std::set<std::string> peerUrls;
    for(const auto& peer : connected) {
        peerUrls.insert(peer.first);
    }

    return peerUrls;
}

uint64_t CryptoKernel::Network::getCurrentHeight() {
    return currentHeight;
}

std::map<std::string, CryptoKernel::Network::peerStats>
CryptoKernel::Network::getPeerStats() {
    std::lock_guard<std::recursive_mutex> lock(connectedMutex);

    std::map<std::string, peerStats> returning;

    for(const auto& peer : connected) {
        peerStats stats = peer.second->peer->getPeerStats();
        stats.version = peer.second->info["version"].asString();
        stats.blockHeight = peer.second->info["height"].asUInt64();
        returning.insert(std::make_pair(peer.first, stats));
    }

    return returning;
}
