#include <chrono>

#include "version.h"
#include "networkpeer.h"

CryptoKernel::Network::Peer::Peer(sf::TcpSocket* client, CryptoKernel::Blockchain* blockchain,
                                  CryptoKernel::Network* network, const bool incoming) {
    this->client = client;
    this->blockchain = blockchain;
    this->network = network;
    running = true;

    const time_t t = std::time(0);
    generator.seed(static_cast<uint64_t> (t));

    stats.connectedSince = t;
    stats.ping = 0;
    stats.transferUp = 0;
    stats.transferDown = 0;
    stats.incoming = incoming;

    requestThread.reset(new std::thread(&CryptoKernel::Network::Peer::requestFunc, this));
}

CryptoKernel::Network::Peer::~Peer() {
    running = false;
    requestThread->join();
    clientMutex.lock();
    client->disconnect();
    delete client;
    clientMutex.unlock();
}

Json::Value CryptoKernel::Network::Peer::sendRecv(const Json::Value& request) {
    std::uniform_int_distribution<uint64_t> distribution(0,
            std::numeric_limits<uint64_t>::max());
    const uint64_t nonce = distribution(generator);

    Json::Value modifiedRequest = request;
    modifiedRequest["nonce"] = nonce;
    requests[nonce] = true;

    sf::Packet packet;
    packet << CryptoKernel::Storage::toString(modifiedRequest, false);

    clientMutex.lock();

    client->setBlocking(false);

    const uint64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>
                               (std::chrono::system_clock::now().time_since_epoch()).count();
    if(client->send(packet) != sf::Socket::Done) {
        running = false;
        clientMutex.unlock();
        throw NetworkError();
    }

    stats.transferUp += packet.getDataSize();

    clientMutex.unlock();

    {
		std::unique_lock<std::mutex> cm(clientMutex);
		std::map<uint64_t, Json::Value>& resp = responses;
		if(responseReady.wait_for(cm, std::chrono::milliseconds(15000), [this, nonce] {
			return this->responses.find(nonce) != this->responses.end();
		})) {
			std::map<uint64_t, Json::Value>::iterator it = responses.find(nonce);
			const uint64_t endTime = std::chrono::duration_cast<std::chrono::milliseconds>
									(std::chrono::system_clock::now().time_since_epoch()).count();
			stats.ping = (stats.ping * 0.8) + ((endTime - startTime) * 0.2);
			const Json::Value returning = it->second;
			it = responses.erase(it);
			cm.unlock();
			return returning;
		}
		else {
			running = false;
			cm.unlock();
			throw NetworkError();
		}
    }
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

    stats.transferUp += packet.getDataSize();

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
            stats.transferDown += packet.getDataSize();

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
            const Json::Value request = CryptoKernel::Storage::toJson(requestString); // but this is the response....

            try {
                if(!request["command"].empty()) {
                    if(request["command"] == "info") {
                        Json::Value response;
                        response["data"]["version"] = version;
                        response["data"]["tipHeight"] = network->getCurrentHeight();
                        for(const auto& peer : network->getConnectedPeers()) {
                            response["data"]["peers"].append(peer);
                        }
                        response["nonce"] = request["nonce"].asUInt64();
                        send(response);
                    } else if(request["command"] == "transactions") {
						std::vector<CryptoKernel::Blockchain::transaction> txs;
						for(unsigned int i = 0; i < request["data"].size(); i++) {
							const CryptoKernel::Blockchain::transaction tx = CryptoKernel::Blockchain::transaction(
										request["data"][i]);

							const auto txResult = blockchain->submitTransaction(tx);

							if(std::get<0>(txResult)) {
								txs.push_back(tx);
							} else if(std::get<1>(txResult)) {
								network->changeScore(client->getRemoteAddress().toString(), 50);
							}
						}

						if(txs.size() > 0) {
							network->broadcastTransactions(txs);
						}
                    } else if(request["command"] == "block") {
						const CryptoKernel::Blockchain::block block = CryptoKernel::Blockchain::block(
									request["data"]);

						// Don't accept blocks that are more than two hours away from the current time
						const int64_t now = std::time(nullptr);
						if(std::abs((int)(now - block.getTimestamp())) > 2 * 60 * 60) {
							network->changeScore(client->getRemoteAddress().toString(), 50);
						} else {
							try {
								blockchain->getBlockDB(block.getId().toString());
							} catch(const CryptoKernel::Blockchain::NotFoundException& e) {
								const auto blockResult = blockchain->submitBlock(block, false);
								if(std::get<0>(blockResult)) {
									network->broadcastBlock(block);
								} else if(std::get<1>(blockResult)) {
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
                        if(end > start && (end - start) <= 5) {
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
                        responseReady.notify_all();
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
        if(timeElapsed >= 30 && (double)nRequests/(double)timeElapsed > 50.0) {
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
        request["data"]["height"] = height;
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
    request["data"]["start"] = start;
    request["data"]["end"] = end;
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

CryptoKernel::Network::peerStats CryptoKernel::Network::Peer::getPeerStats() const {
    return stats;
}
