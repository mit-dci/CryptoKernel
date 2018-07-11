#include "network.h"
#include "networkpeer.h"
#include "version.h"

#include <list>

CryptoKernel::Network::Connection::Connection() {

}

Json::Value CryptoKernel::Network::Connection::getInfo() {
	std::lock_guard<std::mutex> pm(peerMutex);
	return peer->getInfo();
}

void CryptoKernel::Network::Connection::sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction>&
					  transactions) {
	std::lock_guard<std::mutex> pm(peerMutex);
	peer->sendTransactions(transactions);
}

void CryptoKernel::Network::Connection::sendBlock(const CryptoKernel::Blockchain::block& block) {
	std::lock_guard<std::mutex> pm(peerMutex);
	peer->sendBlock(block);
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Network::Connection::getUnconfirmedTransactions() {
	std::lock_guard<std::mutex> pm(peerMutex);
	return peer->getUnconfirmedTransactions();
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Connection::getBlock(const uint64_t height, const std::string& id) {
	std::lock_guard<std::mutex> pm(peerMutex);
	return peer->getBlock(height, id);
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Connection::getBlocks(const uint64_t start,
													   const uint64_t end) {
	std::lock_guard<std::mutex> pm(peerMutex);
	return peer->getBlocks(start, end);
}

void CryptoKernel::Network::Connection::setPeer(CryptoKernel::Network::Peer* peer) {
	std::lock_guard<std::mutex> pm(peerMutex);
	this->peer.reset(peer);
}

CryptoKernel::Network::Connection::~Connection() {

}

CryptoKernel::Network::Network(CryptoKernel::Log* log,
                               CryptoKernel::Blockchain* blockchain,
                               const unsigned int port,
                               const std::string& dbDir) {
    this->log = log;
    this->blockchain = blockchain;
    this->port = port;
    bestHeight = 0;
    currentHeight = 0;

    myAddress = sf::IpAddress::getPublicAddress();

    networkdb.reset(new CryptoKernel::Storage(dbDir, false, 8, false));
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
    //connectionThread.reset(new std::thread(&CryptoKernel::Network::connectionFunc, this));

    // Start management thread
    //networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));

    // Start peer thread
   	makeOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::makeOutgoingConnectionsWrapper, this));

    // Start peer thread
    infoOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::infoOutgoingConnectionsWrapper, this));
}

CryptoKernel::Network::~Network() {
    running = false;
    //connectionThread->join();
    //networkThread->join();
    makeOutgoingConnectionsThread->join();
    infoOutgoingConnectionsThread->join();
    listener.close();
}

void CryptoKernel::Network::makeOutgoingConnectionsWrapper() {
	while(running) {
		makeOutgoingConnections();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void CryptoKernel::Network::infoOutgoingConnectionsWrapper() {
	while(running) {
		bool wait = false;

		infoOutgoingConnections();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void CryptoKernel::Network::makeOutgoingConnections() {
	std::map<std::string, Json::Value> peersToTry;
	{
		//std::lock_guard<std::recursive_mutex> lock(connectedMutex);
		CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(
				peers.get(), networkdb.get());

		for(it->SeekToFirst(); it->Valid(); it->Next()) {
			if(connected.size() >= 8) { // honestly, this is enough
				std::this_thread::sleep_for(std::chrono::seconds(20)); // so stop looking for a while
			}

			Json::Value peerInfo = it->value();

			if(connected.contains(it->key())) {
				continue;
			}

			std::time_t result = std::time(nullptr);

			/*const auto banIt = banned.find(it->key());
			if(banIt != banned.end()) {
				if(banIt->second > static_cast<uint64_t>(result)) {
					continue;
				}
			}*/

			if(peerInfo["lastattempt"].asUInt64() + 5 * 60 > static_cast<unsigned long long int>
					(result) && peerInfo["lastattempt"].asUInt64() != peerInfo["lastseen"].asUInt64()) {
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
			peersToTry.insert(std::pair<std::string, Json::Value>(it->key(), peerInfo));
		}
		delete it;
	}

	// here, we only access local data (except where there are more locks)
	for(std::map<std::string, Json::Value>::iterator entry = peersToTry.begin(); entry != peersToTry.end(); ++entry) {
		std::string peerIp = entry->first;
		Json::Value peerData = entry->second;

		sf::TcpSocket* socket = new sf::TcpSocket();
		if(socket->connect(peerIp, port, sf::seconds(3)) == sf::Socket::Done) {
			log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + peerIp);
			{ // this scoping is unnecessary
				//std::lock_guard<std::recursive_mutex> lock(connectedMutex);
				Connection* connection = new Connection;
				connection->setPeer(new Peer(socket, blockchain, this, false));

				peerData["lastseen"] = static_cast<uint64_t>(std::time(nullptr));
				peerData["score"] = 0;

				connection->setInfo(peerData);

				//connected[peerIp].reset(peerInfo);
				//connected.find(peerIp)->second.reset(peerInfo);
				connected.insert(peerIp, connection);
			}
		}
		else {
			log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + peerIp);
			delete socket;
		}
	}
}

void CryptoKernel::Network::infoOutgoingConnections() {
	//std::lock_guard<std::recursive_mutex> lock(connectedMutex);

	std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

	std::set<std::string> removals;

	auto keys = connected.keys();
	for(auto key: keys) {
		log->printf(LOG_LEVEL_INFO, "WE HAVE KEY " + key);
		auto it = connected.find(key);
		try {
			const Json::Value info = it->second->getInfo();
			try {
				const std::string peerVersion = info["version"].asString();
				if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find("."))) {
					log->printf(LOG_LEVEL_WARN,
								"Network(): " + it->first + " has a different major version than us");
					throw Peer::NetworkError();
				}

				/*const auto banIt = banned.find(it->first);
				if(banIt != banned.end()) {
					if(banIt->second > static_cast<uint64_t>(std::time(nullptr))) {
						log->printf(LOG_LEVEL_WARN,
									"Network(): Disconnecting " + it->first + " for being banned");
						throw Peer::NetworkError();
					}
				}*/

				//it->second->info["height"] = info["tipHeight"].asUInt64();
				it->second->setInfo("height", info["tipHeight"].asUInt64());

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
			//it->second->info["lastseen"] = static_cast<uint64_t>(result);
			it->second->setInfo("lastseen", static_cast<uint64_t>(result));
		} catch(const Peer::NetworkError& e) {
			log->printf(LOG_LEVEL_WARN,
						"Network(): Failed to contact " + it->first + ", disconnecting it");
			removals.insert(it->first);
		}
	}

	for(const auto& peer : removals) {
		const auto it = connected.find(peer);
		peers->put(dbTx.get(), peer, it->second->getInfo());
		if(connected.contains(it->first)) {
			connected.erase(it);
		}
	}

	dbTx->commit();
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

        std::vector<std::string> keys = connected.keys();
        for(auto key : keys) {
        	auto it = connected.find(key); // todo, make sure it exists!
            if(it->second->getInfo("height").asUInt64() > bestHeight) {
                bestHeight = it->second->getInfo("height").asUInt64();
            }
        }

        if(this->currentHeight > bestHeight) {
            bestHeight = this->currentHeight;
        }
        this->bestHeight = bestHeight;
        connectedMutex.unlock();

        log->printf(LOG_LEVEL_INFO,
                    "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " +
                    std::to_string(bestHeight) + ", start height: " + std::to_string(startHeight));

        bool madeProgress = false;

        //Detect if we are behind
        if(bestHeight > currentHeight) {
            connectedMutex.lock();

            keys = connected.keys();
			for(auto key : keys) {
				auto it = connected.find(key);
                if(it->second->getInfo("height").asUInt64() > currentHeight) {
                    std::list<CryptoKernel::Blockchain::block> blocks;

                    const std::string peerUrl = it->first;

                    if(currentHeight == startHeight) {
                        auto nBlocks = 0;
                        do {
                            log->printf(LOG_LEVEL_INFO,
                                        "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " +
                                        std::to_string(currentHeight + 6));
                            try {
                                const auto newBlocks = it->second->getBlocks(currentHeight + 1, currentHeight + 6);
                                nBlocks = newBlocks.size();
                                blocks.insert(blocks.end(), newBlocks.rbegin(), newBlocks.rend());
                                if(nBlocks > 0) {
                                    madeProgress = true;
                                } else {
                                    log->printf(LOG_LEVEL_WARN, "Network(): Peer responded with no blocks");
                                }
                            } catch(Peer::NetworkError& e) {
                                log->printf(LOG_LEVEL_WARN,
                                            "Network(): Failed to contact " + it->first + " " + e.what() +
                                            " while downloading blocks");
                                it++;
                                break;
                            }

                            log->printf(LOG_LEVEL_INFO, "Network(): Testing if we have block " + std::to_string(blocks.rbegin()->getHeight() - 1));

                            try {
                                blockchain->getBlockDB(blocks.rbegin()->getPreviousBlockId().toString());
                            } catch(const CryptoKernel::Blockchain::NotFoundException& e) {
                                if(currentHeight == 1) {
                                    // This peer has a different genesis block to us
                                    changeScore(it->first, 250);
                                    it++;
                                    break;
                                } else {
                                    log->printf(LOG_LEVEL_INFO, "Network(): got block h: " + std::to_string(blocks.rbegin()->getHeight()) + " with prevBlock: " + blocks.rbegin()->getPreviousBlockId().toString() + " prev not found");


                                    currentHeight = std::max(1, (int)currentHeight - nBlocks);
                                    continue;
                                }
                            }

                            break;
                        } while(running);

                        currentHeight += nBlocks;
                    }

                    log->printf(LOG_LEVEL_INFO, "Network(): Found common block " + std::to_string(currentHeight-1) + " with peer, starting block download");

                    while(blocks.size() < 2000 && running && !failure && currentHeight < bestHeight) {
                        log->printf(LOG_LEVEL_INFO,
                                    "Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " +
                                    std::to_string(currentHeight + 6));

                        auto nBlocks = 0;
                        try {
                            const auto newBlocks = it->second->getBlocks(currentHeight + 1, currentHeight + 6);
                            nBlocks = newBlocks.size();
                            blocks.insert(blocks.begin(), newBlocks.rbegin(), newBlocks.rend());
                            if(nBlocks > 0) {
                                madeProgress = true;
                            } else {
                                log->printf(LOG_LEVEL_WARN, "Network(): Peer responded with no blocks");
                                break;
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
                        log->printf(LOG_LEVEL_INFO, "Network(): Waiting for previous block processor thread to finish");
                        blockProcessor->join();
                        blockProcessor.reset();

                        if(failure) {
                            log->printf(LOG_LEVEL_WARN, "Network(): Failure processing blocks");
                            blocks.clear();
                            currentHeight = blockchain->getBlockDB("tip").getHeight();
                            this->currentHeight = currentHeight;
                            startHeight = currentHeight;
                            bestHeight = currentHeight;
                            it++;
                            failure = false;
                            break;
                        }
                    }

                    blockProcessor.reset(new std::thread([&, blocks](const std::string& peer){
                        failure = false;

                        log->printf(LOG_LEVEL_INFO, "Network(): Submitting " + std::to_string(blocks.size()) + " blocks to blockchain");

                        for(auto rit = blocks.rbegin(); rit != blocks.rend() && running; ++rit) {
                            const auto blockResult = blockchain->submitBlock(*rit);

                            if(std::get<1>(blockResult)) {
                                changeScore(peer, 50);
                            }

                            if(!std::get<0>(blockResult)) {
                                failure = true;
                                changeScore(peer, 25);
                                log->printf(LOG_LEVEL_WARN, "Network(): offending block: " + rit->toJson().toStyledString());
                                break;
                            }
                        }
                    }, peerUrl));
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
//    while(running) {
//        sf::TcpSocket* client = new sf::TcpSocket();
//        if(listener.accept(*client) == sf::Socket::Done) {
//            std::lock_guard<std::recursive_mutex> lock(connectedMutex);
//        	//connectedMutex.lock();
//            if(connected.find(client->getRemoteAddress().toString()) != connected.end()) {
//                log->printf(LOG_LEVEL_INFO,
//                            "Network(): Incoming connection duplicates existing connection for " +
//                            client->getRemoteAddress().toString());
//                client->disconnect();
//                delete client;
//                continue;
//            }
//
//            const auto it = banned.find(client->getRemoteAddress().toString());
//            if(it != banned.end()) {
//                if(it->second > static_cast<uint64_t>(std::time(nullptr))) {
//                    log->printf(LOG_LEVEL_INFO,
//                                "Network(): Incoming connection " + client->getRemoteAddress().toString() + " is banned");
//                    client->disconnect();
//                    delete client;
//                    continue;
//                }
//            }
//
//            sf::IpAddress addr(client->getRemoteAddress());
//
//            if(addr == sf::IpAddress::getLocalAddress()
//                    || addr == myAddress
//                    || addr == sf::IpAddress::LocalHost
//                    || addr == sf::IpAddress::None) {
//                log->printf(LOG_LEVEL_INFO,
//                            "Network(): Incoming connection " + client->getRemoteAddress().toString() +
//                            " is connecting to self");
//                client->disconnect();
//                delete client;
//                continue;
//            }
//
//
//            log->printf(LOG_LEVEL_INFO,
//                        "Network(): Peer connected from " + client->getRemoteAddress().toString() + ":" +
//                        std::to_string(client->getRemotePort()));
//            PeerInfo* peerInfo = new PeerInfo();
//            peerInfo->peer.reset(new Peer(client, blockchain, this, true));
//
//            Json::Value info;
//
//            try {
//                info = peerInfo->peer->getInfo();
//            } catch(const Peer::NetworkError& e) {
//                log->printf(LOG_LEVEL_WARN, "Network(): Failed to get information from connecting peer");
//                delete peerInfo;
//                continue;
//            }
//
//            try {
//                peerInfo->info["height"] = info["tipHeight"].asUInt64();
//                peerInfo->info["version"] = info["version"].asString();
//            } catch(const Json::Exception& e) {
//                log->printf(LOG_LEVEL_WARN, "Network(): Incoming peer sent invalid info message");
//                delete peerInfo;
//                continue;
//            }
//
//            const std::time_t result = std::time(nullptr);
//            peerInfo->info["lastseen"] = static_cast<uint64_t>(result);
//
//            peerInfo->info["score"] = 0;
//
//            connected[client->getRemoteAddress().toString()].reset(peerInfo);
//
//            std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());
//            peers->put(dbTx.get(), client->getRemoteAddress().toString(), peerInfo->info);
//            dbTx->commit();
//        } else {
//            delete client;
//            std::this_thread::sleep_for(std::chrono::milliseconds(10));
//        }
//    }
}

unsigned int CryptoKernel::Network::getConnections() {
    return connected.size();
}

void CryptoKernel::Network::broadcastTransactions(const
        std::vector<CryptoKernel::Blockchain::transaction> transactions) {
    /*for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
            it != connected.end(); it++) {
        try {
            it->second->peer->sendTransactions(transactions);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastTransactions(): Failed to contact peer");
        }
    }*/
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block) {
    /*for(std::map<std::string, std::unique_ptr<PeerInfo>>::iterator it = connected.begin();
            it != connected.end(); it++) {
        try {
            it->second->peer->sendBlock(block);
        } catch(CryptoKernel::Network::Peer::NetworkError& err) {
            log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer");
        }
    }*/
}

double CryptoKernel::Network::syncProgress() {
    return (double)(currentHeight)/(double)(bestHeight);
}

void CryptoKernel::Network::changeScore(const std::string& url, const uint64_t score) {
    /*if(connected[url]) {
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
        connected[url]->info["disconnect"] = true;
    }*/
}

std::set<std::string> CryptoKernel::Network::getConnectedPeers() {
    std::set<std::string> peerUrls;
    /*for(const auto& peer : connected) {
        peerUrls.insert(peer.first);
    }*/
    std::vector<std::string> keys = connected.keys();
    for(auto& peer : keys) {
    	peerUrls.insert(peer);
    }

    return peerUrls;
}

uint64_t CryptoKernel::Network::getCurrentHeight() {
    return currentHeight;
}

std::map<std::string, CryptoKernel::Network::peerStats>
CryptoKernel::Network::getPeerStats() {
    std::lock_guard<std::mutex> lock(connectedStatsMutex);

    return connectedStats;
}
