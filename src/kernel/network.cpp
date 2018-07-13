#include "network.h"
#include "networkpeer.h"
#include "version.h"

#include <list>
#include <algorithm>
#include <cstdlib>
#include <ctime>

CryptoKernel::Network::Connection::Connection() {

}

Json::Value CryptoKernel::Network::Connection::getInfo() {
	std::lock_guard<std::mutex> mm(modMutex);
	return peer->getInfo();
}

void CryptoKernel::Network::Connection::sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction>&
					  transactions) {
	std::lock_guard<std::mutex> mm(modMutex);
	peer->sendTransactions(transactions);
}

void CryptoKernel::Network::Connection::sendBlock(const CryptoKernel::Blockchain::block& block) {
	std::lock_guard<std::mutex> mm(modMutex);
	peer->sendBlock(block);
}

std::vector<CryptoKernel::Blockchain::transaction> CryptoKernel::Network::Connection::getUnconfirmedTransactions() {
	std::lock_guard<std::mutex> mm(modMutex);
	return peer->getUnconfirmedTransactions();
}

CryptoKernel::Blockchain::block CryptoKernel::Network::Connection::getBlock(const uint64_t height, const std::string& id) {
	std::lock_guard<std::mutex> mm(modMutex);
	return peer->getBlock(height, id);
}

std::vector<CryptoKernel::Blockchain::block> CryptoKernel::Network::Connection::getBlocks(const uint64_t start,
													   const uint64_t end) {
	std::lock_guard<std::mutex> mm(modMutex);
	return peer->getBlocks(start, end);
}

CryptoKernel::Network::peerStats CryptoKernel::Network::Connection::getPeerStats() {
	std::lock_guard<std::mutex> mm(modMutex);
    return this->getPeerStats();
}

void CryptoKernel::Network::Connection::setPeer(CryptoKernel::Network::Peer* peer) {
	std::lock_guard<std::mutex> mm(modMutex);
	this->peer.reset(peer);
}

bool CryptoKernel::Network::Connection::acquire() {
	if(peerMutex.try_lock()) {
		return true;
	}
	return false;
}

void CryptoKernel::Network::Connection::release() {
	peerMutex.unlock();
}

void CryptoKernel::Network::Connection::setInfo(Json::ArrayIndex& key, Json::Value& value) {
	std::lock_guard<std::mutex> im(infoMutex);
	this->info[key] = value;
}

void CryptoKernel::Network::Connection::setInfo(std::string key, uint64_t value) {
	std::lock_guard<std::mutex> im(infoMutex);
	this->info[key] = value;
}

void CryptoKernel::Network::Connection::setInfo(std::string key, std::string value) {
	std::lock_guard<std::mutex> im(infoMutex);
	this->info[key] = value;
}

void CryptoKernel::Network::Connection::setInfo(Json::Value info) {
	std::lock_guard<std::mutex> im(infoMutex);
	this->info = info;
}

Json::Value& CryptoKernel::Network::Connection::getInfo(Json::ArrayIndex& key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
}

Json::Value& CryptoKernel::Network::Connection::getInfo(std::string key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
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

    std::srand(std::time(0));

    listener.setBlocking(false);

    // Start connection thread
    connectionThread.reset(new std::thread(&CryptoKernel::Network::connectionFunc, this));

    // Start management thread
    networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));

    // Start peer thread
   	makeOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::makeOutgoingConnectionsWrapper, this));

    // Start peer thread
    infoOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::infoOutgoingConnectionsWrapper, this));
}

CryptoKernel::Network::~Network() {
    running = false;
    connectionThread->join();
    networkThread->join();
    makeOutgoingConnectionsThread->join();
    infoOutgoingConnectionsThread->join();
    listener.close();
}

void CryptoKernel::Network::makeOutgoingConnectionsWrapper() {
	while(running) {
		bool wait = false;
		makeOutgoingConnections(wait);
		if(wait) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

void CryptoKernel::Network::infoOutgoingConnectionsWrapper() {
	while(running) {
		infoOutgoingConnections();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void CryptoKernel::Network::makeOutgoingConnections(bool& wait) {
	std::map<std::string, Json::Value> peersToTry;
	{
		CryptoKernel::Storage::Table::Iterator* it = new CryptoKernel::Storage::Table::Iterator(
				peers.get(), networkdb.get());

		for(it->SeekToFirst(); it->Valid(); it->Next()) {
			if(connected.size() >= 8) { // honestly, this is enough
				wait = true;
				return;
			}

			Json::Value peerInfo = it->value();

			if(connected.contains(it->key())) {
				continue;
			}

			std::time_t result = std::time(nullptr);

			const auto banIt = banned.find(it->key());
			if(banIt != banned.end()) {
				if(banIt->second > static_cast<uint64_t>(result)) {
					continue;
				}
			}

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
			Connection* connection = new Connection;
			connection->setPeer(new Peer(socket, blockchain, this, false));

			peerData["lastseen"] = static_cast<uint64_t>(std::time(nullptr));
			peerData["score"] = 0;

			connection->setInfo(peerData);

			connected.at(peerIp).reset(connection);
		}
		else {
			log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + peerIp);
			delete socket;
		}
	}
}

void CryptoKernel::Network::infoOutgoingConnections() {
	std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

	std::set<std::string> removals;

	std::vector<std::string> keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
	for(auto key: keys) {
		auto it = connected.find(key);
		if(it != connected.end() && it->second->acquire()) {
			try {
				const Json::Value info = it->second->getInfo();
				try {
					const std::string peerVersion = info["version"].asString();
					if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find("."))) {
						log->printf(LOG_LEVEL_WARN,
									"Network(): " + it->first + " has a different major version than us");
						throw Peer::NetworkError();
					}

					const auto banIt = banned.find(it->first);
					if(banIt != banned.end()) {
						if(banIt->second > static_cast<uint64_t>(std::time(nullptr))) {
							log->printf(LOG_LEVEL_WARN,
										"Network(): Disconnecting " + it->first + " for being banned");
							throw Peer::NetworkError();
						}
					}

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
				it->second->setInfo("lastseen", static_cast<uint64_t>(result));
			} catch(const Peer::NetworkError& e) {
				log->printf(LOG_LEVEL_WARN,
							"Network(): Failed to contact " + it->first + ", disconnecting it");

				//removals.insert(it->first);
				connected.erase(it);
				continue;
			}
			it->second->release();
		}
	}

	/*for(const auto& peer : removals) {
		const auto it = connected.find(peer);
		peers->put(dbTx.get(), peer, it->second->getInfo()); // how did this work???  this only winds up getting called if info wasn't fetched in the first place
		if(connected.contains(it->first)) {
			connected.erase(it);
		}
	}*/

	connectedStats.clear();
	keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
	for(const std::string key : keys) {
		auto it = connected.find(key);
		if(it != connected.end() && it->second->acquire()) {
			peerStats stats = it->second->getPeerStats();
			stats.version = it->second->getInfo("version").asString();
			stats.blockHeight = it->second->getInfo("height").asUInt64();
			connectedStats.insert(it->first, stats);
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
        uint64_t bestHeight = currentHeight;

        std::vector<std::string> keys = connected.keys();
        std::random_shuffle(keys.begin(), keys.end());
        for(auto key : keys) {
        	auto it = connected.find(key);
        	if(it != connected.end() && it->second->acquire()) {
        		if(it->second->getInfo("height").asUInt64() > bestHeight) {
					bestHeight = it->second->getInfo("height").asUInt64();
				}
        		it->second->release();
        	}
        }

        if(this->currentHeight > bestHeight) { // todo, protect this
            bestHeight = this->currentHeight;
        }
        this->bestHeight = bestHeight;

        log->printf(LOG_LEVEL_INFO,
                    "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " +
                    std::to_string(bestHeight) + ", start height: " + std::to_string(startHeight));

        bool madeProgress = false;

        //Detect if we are behind
        if(bestHeight > currentHeight) {
            //connectedMutex.lock();

            keys = connected.keys();
            std::random_shuffle(keys.begin(), keys.end());
			for(auto key : keys) {
				auto it = connected.find(key);
				if(it != connected.end() && it->second->acquire()) {
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
					}
					it->second->release();
				}
            }
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
            if(connected.contains(client->getRemoteAddress().toString())) {
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
            Connection* connection = new Connection(); // todo check for thread safety (think it's okay)
            connection->setPeer(new Peer(client, blockchain, this, true));

            Json::Value info;

            try {
                info = connection->getInfo();
            } catch(const Peer::NetworkError& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): Failed to get information from connecting peer");
                delete connection;
                continue;
            }

            try {
                connection->setInfo("height", info["tipHeight"].asUInt64());
                connection->setInfo("version", info["version"].asString());
            } catch(const Json::Exception& e) {
                log->printf(LOG_LEVEL_WARN, "Network(): Incoming peer sent invalid info message");
                delete connection;
                continue;
            }

            const std::time_t result = std::time(nullptr);
            connection->setInfo("lastseen", static_cast<uint64_t>(result));

            connection->setInfo("score", 0);

            connected.at(client->getRemoteAddress().toString()).reset(connection);

            std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());
            peers->put(dbTx.get(), client->getRemoteAddress().toString(), connection->getInfo());
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
	std::vector<std::string> keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
	for(std::string key : keys) {
		auto it = connected.find(key);
		if(it != connected.end() && it->second->acquire()) {
			try {
				it->second->sendTransactions(transactions);
			} catch(CryptoKernel::Network::Peer::NetworkError& err) {
				log->printf(LOG_LEVEL_WARN, "Network::broadcastTransactions(): Failed to contact peer");
			}
		}
    }
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block) {
	std::vector<std::string> keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
    for(std::string key : keys) {
    	auto it = connected.find(key);
    	if(it != connected.end() && it->second->acquire()) {
    		try {
				it->second->sendBlock(block);
			} catch(CryptoKernel::Network::Peer::NetworkError& err) {
				log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer");
			}
			it->second->release();
    	}
    }
}

double CryptoKernel::Network::syncProgress() {
    return (double)(currentHeight)/(double)(bestHeight);
}

void CryptoKernel::Network::changeScore(const std::string& url, const uint64_t score) {
    if(connected.contains(url)) { // this check isn't necessary
        connected.at(url)->setInfo("score", connected.at(url)->getInfo("score").asUInt64() + score);
        log->printf(LOG_LEVEL_WARN,
                    "Network(): " + url + " misbehaving, increasing ban score by " + std::to_string(
                        score) + " to " + connected.at(url)->getInfo("score").asString());
        if(connected.at(url)->getInfo("score").asUInt64() > 200) {
            log->printf(LOG_LEVEL_WARN,
                        "Network(): Banning " + url + " for being above the ban score threshold");
            // Ban for 24 hours
            banned.insert(url, static_cast<uint64_t>(std::time(nullptr)) + 24 * 60 * 60);
        }
        connected.at(url)->setInfo("disconnect", true);
    }
}

std::set<std::string> CryptoKernel::Network::getConnectedPeers() {
    std::set<std::string> peerUrls;
    std::vector<std::string> keys = connected.keys();
    std::random_shuffle(keys.begin(), keys.end());
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
    return connectedStats.copyMap();
}
