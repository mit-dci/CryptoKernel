#include <chrono>

#include "version.h"
#include "networkpeer.h"

CryptoKernel::Network::Peer::Peer(sf::TcpSocket* client,
                                  CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network) {
    this->client = client;
    this->blockchain = blockchain;
    this->network = network;
    running = true;

    time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));

    requestThread.reset(new std::thread(&CryptoKernel::Network::Peer::requestFunc, this));
}

CryptoKernel::Network::Peer::~Peer() {
    clientMutex.lock();
    running = false;
    requestThread->join();
    client->disconnect();
    delete client;
    clientMutex.unlock();
}

Json::Value CryptoKernel::Network::Peer::sendRecv(const Json::Value& request) {
    std::uniform_int_distribution<uint64_t> distribution(0,
            std::numeric_limits<uint64_t>::max());
    for(unsigned int tries = 0; tries < 3; tries++) {
        const uint64_t nonce = distribution(generator);

        Json::Value modifiedRequest = request;
        modifiedRequest["nonce"] = static_cast<unsigned long long int>(nonce);
        requests[nonce] = true;

        sf::Packet packet;
        packet << CryptoKernel::Storage::toString(modifiedRequest, false);

        clientMutex.lock();

        client->setBlocking(true);

        if(client->send(packet) != sf::Socket::Done) {
            running = false;
            clientMutex.unlock();
            throw NetworkError();
        }

        clientMutex.unlock();

        for(unsigned int t = 0; t < 120; t++) {
            clientMutex.lock();
            std::map<uint64_t, Json::Value>::iterator it = responses.find(nonce);
            if(it != responses.end()) {
                const Json::Value returning = it->second;
                it = responses.erase(it);
                clientMutex.unlock();
                return returning;
            }
            clientMutex.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }

    running = false;
    clientMutex.unlock();
    throw NetworkError();
}

void CryptoKernel::Network::Peer::send(const Json::Value& response) {
    sf::Packet packet;
    packet << CryptoKernel::Storage::toString(response, false);

    clientMutex.lock();
    client->setBlocking(true);
    if(client->send(packet) != sf::Socket::Done) {
        running = false;
        clientMutex.unlock();
        throw NetworkError();
    }
    clientMutex.unlock();
}

void CryptoKernel::Network::Peer::requestFunc() {
    uint64_t nRequests = 0;
    uint64_t startTime = static_cast<uint64_t>(std::time(nullptr));

    while(running) {
        sf::Packet packet;

        clientMutex.lock();
        client->setBlocking(false);

        if(client->receive(packet) == sf::Socket::Done) {
            nRequests++;

            clientMutex.unlock();

            // Don't allow packets bigger than 50MB
            if(packet.getDataSize() > 50 * 1024 * 1024) {
                network->changeScore(client->getRemoteAddress().toString(), 250);
                running = false;
                break;
            }

            std::string requestString;
            packet >> requestString;

            // If this breaks, request will be null
            const Json::Value request = CryptoKernel::Storage::toJson(requestString);

            try {
                if(!request["command"].empty()) {
                    const double syncProgress = network->syncProgress();
                    if(request["command"] == "info") {
                        Json::Value response;
                        response["data"]["version"] = version;
                        response["data"]["tipHeight"] = static_cast<unsigned long long int>
                                                        (network->getCurrentHeight());
                        for(const auto& peer : network->getConnectedPeers()) {
                            response["data"]["peers"].append(peer);
                        }
                        response["nonce"] = request["nonce"].asUInt64();
                        send(response);
                    } else if(request["command"] == "transactions") {
                        if(syncProgress >= 0.99) {
                            std::vector<CryptoKernel::Blockchain::transaction> txs;
                            for(unsigned int i = 0; i < request["data"].size(); i++) {
                                const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::transaction(
                                            request["data"][i]);
                                const std::set<CryptoKernel::Blockchain::transaction> unconfirmedTransactions =
                                    blockchain->getUnconfirmedTransactions();
                                bool found = false;
                                for(const CryptoKernel::Blockchain::transaction& utx : unconfirmedTransactions) {
                                    if(utx.getId() == tx.getId()) {
                                        found = true;
                                        break;
                                    }
                                }
                                if(blockchain->submitTransaction(tx) && !found) {
                                    txs.push_back(tx);
                                } else if(!found) {
                                    network->changeScore(client->getRemoteAddress().toString(), 50);
                                }
                            }

                            if(txs.size() > 0) {
                                network->broadcastTransactions(txs);
                            }
                        }
                    } else if(request["command"] == "block") {
                        if(syncProgress >= 0.99) {
                            const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::block(
                                        request["data"]);

                            try {
                                blockchain->getBlockDB(block.getId().toString());
                            } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                if(blockchain->submitBlock(block, false)) {
                                    network->broadcastBlock(block);
                                } else {
                                    network->changeScore(client->getRemoteAddress().toString(), 50);
                                }
                            }
                        }
                    } else if(request["command"] == "getunconfirmed") {
                        const std::set<CryptoKernel::Blockchain::transaction> unconfirmedTransactions =
                            blockchain->getUnconfirmedTransactions();
                        Json::Value response;
                        for(const CryptoKernel::Blockchain::transaction& tx : unconfirmedTransactions) {
                            response["data"].append(tx.toJson());
                        }

                        response["nonce"] = request["nonce"].asUInt64();

                        send(response);
                    } else if(request["command"] == "getblocks") {
                        const uint64_t start = request["data"]["start"].asUInt64();
                        const uint64_t end = request["data"]["end"].asUInt64();
                        if(end > start && (end - start) <= 500) {
                            Json::Value returning;
                            for(unsigned int i = start; i < end; i++) {
                                try {
                                    returning["data"].append(blockchain->getBlockByHeight(i).toJson());
                                } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                    break;
                                }
                            }

                            returning["nonce"] = request["nonce"].asUInt64();

                            send(returning);
                        } else {
                            Json::Value response;
                            response["nonce"] = request["nonce"].asUInt64();
                            send(response);
                        }
                    } else if(request["command"] == "getblock") {
                        if(request["data"]["id"].empty()) {
                            Json::Value response;
                            try {
                                response["data"] = blockchain->getBlockByHeight(
                                                       request["data"]["height"].asUInt64()).toJson();
                            } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                response["data"] = Json::Value();
                            }

                            response["nonce"] = request["nonce"].asUInt64();

                            send(response);
                        } else {
                            Json::Value response;
                            try {
                                response["data"] = blockchain->getBlock(request["data"]["id"].asString()).toJson();
                            } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                response["data"] = Json::Value();
                            }
                            response["nonce"] = request["nonce"].asUInt64();
                            send(response);
                        }
                    } else {
                        network->changeScore(client->getRemoteAddress().toString(), 50);
                    }
                } else if(!request["nonce"].empty()) {
                    std::lock_guard<std::mutex> lock(clientMutex);
                    const auto it = requests.find(request["nonce"].asUInt64());
                    if(it != requests.end()) {
                        responses[request["nonce"].asUInt64()] = request["data"];
                        requests.erase(it);
                    } else {
                        network->changeScore(client->getRemoteAddress().toString(), 50);
                    }
                }
            } catch(const NetworkError& e) {
                running = false;
            } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
                network->changeScore(client->getRemoteAddress().toString(), 50);
            } catch(const Json::Exception& e) {
                network->changeScore(client->getRemoteAddress().toString(), 250);
            }
        } else {
            clientMutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const uint64_t timeElapsed = static_cast<uint64_t>(std::time(nullptr)) - startTime;
        if(timeElapsed >= 30 && (double)nRequests/(double)timeElapsed > 1.0) {
            network->changeScore(client->getRemoteAddress().toString(), 20);
            nRequests = 0;
            startTime += timeElapsed;
        }
    }
}

Json::Value CryptoKernel::Network::Peer::getInfo() {
    Json::Value request;
    request["command"] = "info";

    return sendRecv(request);
}

void CryptoKernel::Network::Peer::sendTransactions(const
        std::vector<CryptoKernel::Blockchain::transaction>& transactions) {
    Json::Value request;
    request["command"] = "transactions";
    for(const CryptoKernel::Blockchain::transaction& tx : transactions) {
        request["data"].append(tx.toJson());
    }

    send(request);
}

void CryptoKernel::Network::Peer::sendBlock(const CryptoKernel::Blockchain::block&
        block) {
    Json::Value request;
    request["command"] = "block";
    request["data"] = block.toJson();

    send(request);
}

std::vector<CryptoKernel::Blockchain::transaction>
CryptoKernel::Network::Peer::getUnconfirmedTransactions() {
    Json::Value request;
    request["command"] = "getunconfirmed";
    Json::Value unconfirmed = sendRecv(request);

    std::vector<CryptoKernel::Blockchain::transaction> returning;
    for(unsigned int i = 0; i < unconfirmed.size(); i++) {
        try {
            returning.push_back(CryptoKernel::Blockchain::transaction(unconfirmed[i]));
        } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
            network->changeScore(client->getRemoteAddress().toString(), 50);
            throw NetworkError();
        }
    }

    return returning;
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Peer::getBlock(
    const uint64_t height, const std::string& id) {
    Json::Value request;
    request["command"] = "getblock";
    Json::Value block;
    if(id != "") {
        request["data"]["id"] = id;
        block = sendRecv(request);
    } else {
        request["data"]["height"] = static_cast<unsigned long long int>(height);
        block = sendRecv(request);
    }

    try {
        return CryptoKernel::Blockchain::block(block);
    } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
        network->changeScore(client->getRemoteAddress().toString(), 50);
        throw NetworkError();
    }
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Peer::getBlocks(
    const uint64_t start, const uint64_t end) {
    Json::Value request;
    request["command"] = "getblocks";
    request["data"]["start"] = static_cast<unsigned long long int>(start);
    request["data"]["end"] = static_cast<unsigned long long int>(end);
    Json::Value blocks = sendRecv(request);

    std::vector<CryptoKernel::Blockchain::block> returning;
    for(unsigned int i = 0; i < blocks.size(); i++) {
        try {
            returning.push_back(CryptoKernel::Blockchain::block(blocks[i]));
        } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
            network->changeScore(client->getRemoteAddress().toString(), 50);
            throw NetworkError();
        }
    }

    return returning;
}
