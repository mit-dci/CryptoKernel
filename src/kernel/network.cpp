#include "network.h"
#include "networkpeer.h"
#include "version.h"

#include <list>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <openssl/rand.h>

CryptoKernel::Network::Connection::Connection() {

}

Json::Value CryptoKernel::Network::Connection::getInfo() {
	std::lock_guard<std::mutex> mm(modMutex);
	return peer->getInfo();
}

Json::Value CryptoKernel::Network::Connection::getCachedInfo() {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info;
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
	return peer->getPeerStats();
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

Json::Value CryptoKernel::Network::Connection::getInfo(Json::ArrayIndex& key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
}

Json::Value CryptoKernel::Network::Connection::getInfo(std::string key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
}

void CryptoKernel::Network::Connection::setSendCipher(NoiseCipherState* cipher) {
	std::lock_guard<std::mutex> pm(peerMutex);
	peer->setSendCipher(cipher);
}

void CryptoKernel::Network::Connection::setRecvCipher(NoiseCipherState* cipher) {
	std::lock_guard<std::mutex> pm(peerMutex);
	peer->setRecvCipher(cipher);
}

CryptoKernel::Network::Connection::~Connection() {
    std::lock_guard<std::mutex> pm(peerMutex);
    std::lock_guard<std::mutex> im(infoMutex);
    std::lock_guard<std::mutex> mm(modMutex);
}

CryptoKernel::Network::Network(CryptoKernel::Log* log,
                               CryptoKernel::Blockchain* blockchain,
                               const unsigned int port,
                               const std::string& dbDir) {
    this->log = log;
    this->blockchain = blockchain;
    this->port = port;
	std::lock_guard<std::mutex> lock(heightMutex);
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

	unsigned char seedBuf[64];
	if(!RAND_bytes(seedBuf, sizeof(seedBuf))) {
		throw std::runtime_error("Could not randomize connections");
	}
	uint64_t seed;
	memcpy(&seed, seedBuf, sizeof(seedBuf) / 8);
    std::srand(seed);

    // Start connection thread
    connectionThread.reset(new std::thread(&CryptoKernel::Network::connectionFunc, this));

    // Start management thread
    networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));

    // Start peer thread
   	makeOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::makeOutgoingConnectionsWrapper, this));

    // Start peer thread
    infoOutgoingConnectionsThread.reset(new std::thread(&CryptoKernel::Network::infoOutgoingConnectionsWrapper, this));

	incomingEncryptionHandshakeThread.reset(new std::thread(&CryptoKernel::Network::incomingEncryptionHandshakeWrapper, this));
	outgoingEncryptionHandshakeThread.reset(new std::thread(&CryptoKernel::Network::outgoingEncryptionHandshakeWrapper, this));

	postHandshakeConnectThread.reset(new std::thread(&CryptoKernel::Network::postHandshakeConnect, this));
}

CryptoKernel::Network::~Network() {
    running = false;
    connectionThread->join();
    networkThread->join();
    makeOutgoingConnectionsThread->join();
    infoOutgoingConnectionsThread->join();

	incomingEncryptionHandshakeThread->join();
	outgoingEncryptionHandshakeThread->join();
	postHandshakeConnectThread->join();

	connectedPending.clear();
	connected.clear();
	handshakeClients.clear();
	handshakeServers.clear();

    listener.close();
}

void CryptoKernel::Network::makeOutgoingConnectionsWrapper() {
	while(running) {
		bool wait = false;
		makeOutgoingConnections(wait);
		if(wait) {
			std::this_thread::sleep_for(std::chrono::seconds(20)); // stop looking for a while
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(4000));
		}
	}
}

void CryptoKernel::Network::infoOutgoingConnectionsWrapper() {
	while(running) {
		infoOutgoingConnections();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // just do this once every two seconds
	}
}

void CryptoKernel::Network::makeOutgoingConnections(bool& wait) {
	std::map<std::string, Json::Value> peersToTry;
	std::vector<std::string> peerIps;

	std::unique_ptr<Storage::Transaction> dbTx(networkdb->beginReadOnly());

	std::unique_ptr<Storage::Table::Iterator> it(new Storage::Table::Iterator(peers.get(), networkdb.get(), dbTx->snapshot));

	for(it->SeekToFirst(); it->Valid(); it->Next()) {
		if(connected.size() >= 8) { // honestly, this is enough
			wait = true;
			it.reset();
			return;
		}

		Json::Value peerInfo = it->value();

		if(connected.contains(it->key()) || connectedPending.contains(it->key())) {
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
				|| addr == sf::IpAddress::None
				|| addr == sf::IpAddress::Any) {
			continue;
		}

		peersToTry.insert(std::pair<std::string, Json::Value>(it->key(), peerInfo));
		peerIps.push_back(it->key());
	}
	it.reset();

	std::random_shuffle(peerIps.begin(), peerIps.end());
	for(std::string peerIp : peerIps) {
		if(!running) {
			break;
		}
		auto entry = peersToTry.find(peerIp);
		Json::Value peerData = entry->second;

		sf::TcpSocket* socket = new sf::TcpSocket();
		log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + peerIp);
		if(socket->connect(peerIp, port, sf::seconds(3)) == sf::Socket::Done) {
			log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + peerIp);
			Connection* connection = new Connection;

			connection->setPeer(new Peer(socket, blockchain, this, false, log));

			peerData["lastseen"] = static_cast<uint64_t>(std::time(nullptr));
			peerData["score"] = 0;

			connection->setInfo(peerData);

			connectedPending.insert(peerIp, std::shared_ptr<Connection>(connection));
			
			peersToQuery.insert(std::make_pair(peerIp, true));
		}
		else {
			log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + peerIp);
			delete socket;
		}
	}
}

void CryptoKernel::Network::postHandshakeConnect() {
	while(running) {
		std::vector<std::string> keys = handshakeClients.keys();
		std::random_shuffle(keys.begin(), keys.end());
		for(std::string key: keys) {
			auto it = handshakeClients.atMaybe(key);
			if(it.first) {
				std::lock_guard<std::mutex> hsm(handshakeMutex);
				if(it.second->getHandshakeSuccess()) {
					transferConnection(key, it.second->send_cipher, it.second->recv_cipher);
					handshakeClients.erase(key);
				}
				else if(it.second->getHandshakeComplete() && !it.second->getHandshakeSuccess()) {
					// handshake failed
					handshakeClients.erase(key);
					connectedPending.erase(key);
				}
			}
		}

		keys = handshakeServers.keys();
		std::random_shuffle(keys.begin(), keys.end());
		for(std::string key: keys) {
			auto it = handshakeServers.atMaybe(key);
			if(it.first) {
				std::lock_guard<std::mutex> hsm(handshakeMutex);
				if(it.second->getHandshakeSuccess()) {
					transferConnection(key, it.second->sendCipher, it.second->recvCipher);
					handshakeServers.erase(key);
				}
				else if(it.second->getHandshakeComplete() && !it.second->getHandshakeSuccess()) {
					// handshake failed
					handshakeServers.erase(key);
					connectedPending.erase(key);
				}
			}
		}

		keys = plaintextHosts.keys();
		std::random_shuffle(keys.begin(), keys.end());
		for(std::string key: keys) {
			transferConnection(key);
			plaintextHosts.erase(key);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200)); // arbitrary
	}
}

void CryptoKernel::Network::transferConnection(std::string addr, NoiseCipherState* send_cipher, NoiseCipherState* recv_cipher) {
	auto it = connectedPending.atMaybe(addr);
	if(it.first) {
		log->printf(LOG_LEVEL_INFO, "Network(): Transferring " + addr + " from pending to connected");
		Connection* connection = it.second.get();
		connection->setSendCipher(send_cipher);
		connection->setRecvCipher(recv_cipher);
		connected.insert(addr, it.second);
		connectedPending.erase(addr);
	}
}

void CryptoKernel::Network::infoOutgoingConnections() {
	std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());

	std::set<std::string> removals;

	std::vector<std::string> keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
	for(auto key: keys) {
		auto it = connected.atMaybe(key);
		if(it.first) {
			try {
				const Json::Value info = it.second->getInfo();
				try {
					const std::string peerVersion = info["version"].asString();
					if(peerVersion.substr(0, peerVersion.find(".")) != version.substr(0, version.find("."))) {
						log->printf(LOG_LEVEL_WARN,
									"Network(): " + key + " has a different major version than us");
						throw Peer::NetworkError("peer has an incompatible major version");
					}

					const auto banIt = banned.find(key);
					if(banIt != banned.end()) {
						if(banIt->second > static_cast<uint64_t>(std::time(nullptr))) {
							log->printf(LOG_LEVEL_WARN,
										"Network(): Disconnecting " + key + " for being banned");
							throw Peer::NetworkError("peer is banned");
						}
					}

                    it.second->setInfo("version", info["version"].asString());
					it.second->setInfo("height", info["tipHeight"].asUInt64());

					// update connected stats
					peerStats stats = it.second->getPeerStats();
					stats.version = it.second->getInfo("version").asString();
					stats.blockHeight = it.second->getInfo("height").asUInt64();
					connectedStats.insert(std::make_pair(key, stats));

					for(const Json::Value& peer : info["peers"]) {
						sf::IpAddress addr(peer.asString());
						if(addr != sf::IpAddress::None && addr != sf::IpAddress::Any) {
							if(!peers->get(dbTx.get(), addr.toString()).isObject()) {
								log->printf(LOG_LEVEL_INFO, "Network(): Discovered new peer: " + addr.toString());
								Json::Value newSeed;
								newSeed["lastseen"] = 0;
								newSeed["height"] = 1;
								newSeed["score"] = 0;
								peers->put(dbTx.get(), addr.toString(), newSeed);
							}
						} else {
							changeScore(key, 10);
							throw Peer::NetworkError("peer sent a malformed peer IP address: \"" + addr.toString() + "\"");
						}
					}
				} catch(const Json::Exception& e) {
					changeScore(key, 50);
					throw Peer::NetworkError("peer sent a malformed info message");
				}

				const std::time_t result = std::time(nullptr);
				it.second->setInfo("lastseen", static_cast<uint64_t>(result));
			} catch(const Peer::NetworkError& e) {
				log->printf(LOG_LEVEL_WARN,
							"Network(): Failed to contact " + key + ", disconnecting it for: " + e.what());

				peers->put(dbTx.get(), key, it.second->getCachedInfo());
				connectedStats.erase(key);
				connected.erase(key);
				continue;
			}
		}
	}

	dbTx->commit();
}

void CryptoKernel::Network::networkFunc() {
    std::unique_ptr<std::thread> blockProcessor;
    bool failure = false;
	heightMutex.lock();
	uint64_t currentHeight = blockchain->getBlockDB("tip").getHeight();
	this->currentHeight = currentHeight;
	uint64_t startHeight = currentHeight;
	heightMutex.unlock();

    while(running) {
        //Determine best chain
        uint64_t bestHeight = currentHeight;

        std::vector<std::string> keys = connected.keys();
        std::random_shuffle(keys.begin(), keys.end());
        for(auto key : keys) {
        	auto it = connected.atMaybe(key);
        	if(it.first) {
        		if(it.second->getInfo("height").asUInt64() > bestHeight) {
					bestHeight = it.second->getInfo("height").asUInt64();
				}
        	}
        }

		heightMutex.lock();
        if(this->currentHeight > bestHeight) {
            bestHeight = this->currentHeight;
        }
        this->bestHeight = bestHeight;
		heightMutex.unlock();

        log->printf(LOG_LEVEL_INFO,
                    "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " +
                    std::to_string(bestHeight) + ", start height: " + std::to_string(startHeight));

        bool madeProgress = false;

        //Detect if we are behind
        if(bestHeight > currentHeight) {
            keys = connected.keys();
            std::random_shuffle(keys.begin(), keys.end());
			for(auto key : keys) {
				auto it = connected.atMaybe(key);
				if(it.first) {
					if(it.second->getInfo("height").asUInt64() > currentHeight) {
						std::list<CryptoKernel::Blockchain::block> blocks;

						const std::string peerUrl = key;

						if(currentHeight == startHeight) {
							auto nBlocks = 0;
							do {
								log->printf(LOG_LEVEL_INFO,
											"Network(): Downloading blocks " + std::to_string(currentHeight + 1) + " to " +
											std::to_string(currentHeight + 6));
								try {
									const auto newBlocks = it.second->getBlocks(currentHeight + 1, currentHeight + 6);
									nBlocks = newBlocks.size();
									blocks.insert(blocks.end(), newBlocks.rbegin(), newBlocks.rend());
									if(nBlocks > 0) {
										madeProgress = true;
									} else {
										log->printf(LOG_LEVEL_WARN, "Network(): Peer responded with no blocks");
									}
								} catch(const Peer::NetworkError& e) {
									log->printf(LOG_LEVEL_WARN,
												"Network(): Failed to contact " + peerUrl + " " + e.what() +
												" while downloading blocks");
									break;
								}

								log->printf(LOG_LEVEL_INFO, "Network(): Testing if we have block " + std::to_string(blocks.rbegin()->getHeight() - 1));

								try {
									blockchain->getBlockDB(blocks.rbegin()->getPreviousBlockId().toString());
								} catch(const CryptoKernel::Blockchain::NotFoundException& e) {
									if(currentHeight == 1) {
										// This peer has a different genesis block to us
										changeScore(peerUrl, 250);
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
								const auto newBlocks = it.second->getBlocks(currentHeight + 1, currentHeight + 6);
								nBlocks = newBlocks.size();
								blocks.insert(blocks.begin(), newBlocks.rbegin(), newBlocks.rend());
								if(nBlocks > 0) {
									madeProgress = true;
								} else {
									log->printf(LOG_LEVEL_WARN, "Network(): Peer responded with no blocks");
									break;
								}
							} catch(const Peer::NetworkError& e) {
								log->printf(LOG_LEVEL_WARN,
											"Network(): Failed to contact " + peerUrl + " " + e.what() +
											" while downloading blocks");
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
								heightMutex.lock();
								this->currentHeight = currentHeight;
								heightMutex.unlock();
								startHeight = currentHeight;
								bestHeight = currentHeight;
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
									changeScore(peer, 250);
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
				}
            }
        }

        if(bestHeight <= currentHeight || connected.size() == 0 || !madeProgress) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20000));
            currentHeight = blockchain->getBlockDB("tip").getHeight();
            startHeight = currentHeight;
            heightMutex.lock();
			this->currentHeight = currentHeight;
			heightMutex.unlock();
        }
    }

    if(blockProcessor) {
        blockProcessor->join();
    }
}

void CryptoKernel::Network::incomingEncryptionHandshakeWrapper() {
	sf::TcpListener ls;
	sf::SocketSelector selector;
	if(ls.listen(port + 1) != sf::Socket::Done) {
		log->printf(LOG_LEVEL_ERR, "Network(): Could not bind to port " + std::to_string(port + 1));
	}
	selector.add(ls);

	while(running) {
		incomingEncryptionHandshakeFunc(ls, selector);
	}
}

void CryptoKernel::Network::incomingEncryptionHandshakeFunc(sf::TcpListener& ls, sf::SocketSelector& selector) {
	if(selector.wait(sf::seconds(2))) {
		if(selector.isReady(ls)) { // don't need this outer if anymore
			sf::TcpSocket* client = new sf::TcpSocket();
			if(ls.accept(*client) == sf::Socket::Done) {
				std::string addr = client->getRemoteAddress().toString();
				log->printf(LOG_LEVEL_INFO, "Network(): Connection accepted from " + addr);
				addToNoisePool(client);
			} else {
				log->printf(LOG_LEVEL_WARN, "Network(): Failed to accept incoming connection");
				delete client;
			}
		}
	}
}

void CryptoKernel::Network::outgoingEncryptionHandshakeWrapper() {
	while(running) {
		outgoingEncryptionHandshakeFunc();
	}
}

void CryptoKernel::Network::outgoingEncryptionHandshakeFunc() {
	std::vector<std::string> addresses = peersToQuery.keys();
	std::random_shuffle(addresses.begin(), addresses.end());

	for(std::string addr : addresses) {
		sf::TcpSocket* client = new sf::TcpSocket();
		log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + addr + " to query encryption preference");
		if(client->connect(addr, port + 1, sf::seconds(3)) != sf::Socket::Done) {
			log->printf(LOG_LEVEL_INFO, "Network(): Couldn't query " + addr + " for encryption preference (it likely doesn't support encryption)");
			peersToQuery.erase(addr);
			plaintextHosts.insert(addr, true);
            delete client;
			continue;
		}
		addToNoisePool(client);
		peersToQuery.erase(addr);
	}

	if(addresses.empty()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void CryptoKernel::Network::addToNoisePool(sf::TcpSocket* socket) {
	std::lock_guard<std::mutex> hsm(handshakeMutex);

	std::string addr = socket->getRemoteAddress().toString();

	log->printf(LOG_LEVEL_INFO, "adding " + addr + " to noise");

	if(!handshakeServers.contains(addr) && !handshakeClients.contains(addr) && !connected.contains(addr)) {
		if(addr < myAddress.toString()) {
			log->printf(LOG_LEVEL_INFO, "Network(): Adding a noise client (to handshakeServers), " + addr);
			handshakeServers.insert(addr, std::shared_ptr<NoiseServer>(new NoiseServer(socket, log)));
		}
		else {
			log->printf(LOG_LEVEL_INFO, "Network(): Added connection to handshake clients: " + addr);
			handshakeClients.insert(addr, std::shared_ptr<NoiseClient>(new NoiseClient(socket, log)));
		}
	}
	else {
		// this seems to cause unexplained problems (although they are handled)
		socket->disconnect();
		delete socket;
	}
}

void CryptoKernel::Network::connectionFunc() {
    while(running) {
        sf::SocketSelector selector;
        selector.add(listener);
        sf::TcpSocket* client = new sf::TcpSocket();
        if(selector.wait(sf::seconds(2))) {
            if(listener.accept(*client) != sf::Socket::Done) {
                delete client;
                continue;
            }

			const sf::IpAddress addr(client->getRemoteAddress());

            if(connected.contains(addr.toString()) || connectedPending.contains(addr.toString())) {
                log->printf(LOG_LEVEL_INFO,
                            "Network(): Incoming connection duplicates existing connection for " +
                            addr.toString());
                client->disconnect();
                delete client;
                continue;
            }

            const auto it = banned.find(addr.toString());
            if(it != banned.end()) {
                if(it->second > static_cast<uint64_t>(std::time(nullptr))) {
                    log->printf(LOG_LEVEL_INFO,
                                "Network(): Incoming connection " + addr.toString() + " is banned");
                    client->disconnect();
                    delete client;
                    continue;
                }
            }

            if(addr == sf::IpAddress::getLocalAddress()
                    || addr == myAddress
                    || addr == sf::IpAddress::LocalHost
                    || addr == sf::IpAddress::None
					|| addr == sf::IpAddress::Any) {
                log->printf(LOG_LEVEL_INFO,
                            "Network(): Incoming connection " + addr.toString() +
                            " is connecting to self");
                client->disconnect();
                delete client;
                continue;
            }


            log->printf(LOG_LEVEL_INFO,
                        "Network(): Peer connected from " + addr.toString() + ":" +
                        std::to_string(client->getRemotePort()));
            Connection* connection = new Connection();
            connection->setPeer(new Peer(client, blockchain, this, true, log));

            Json::Value info;
            const std::time_t result = std::time(nullptr);

            connection->setInfo("lastseen", static_cast<uint64_t>(result));
            connection->setInfo("score", 0);

            connectedPending.insert(addr.toString(), std::shared_ptr<Connection>(connection));
			peersToQuery.insert(std::make_pair(addr.toString(), true));
        } else {
            delete client;
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
		auto it = connected.atMaybe(key);
		if(it.first) {
			try {
				it.second->sendTransactions(transactions);
			} catch(const Peer::NetworkError& err) {
				log->printf(LOG_LEVEL_WARN, "Network::broadcastTransactions(): Failed to contact peer: " + std::string(err.what()));
			}
		}
    }
}

void CryptoKernel::Network::broadcastBlock(const CryptoKernel::Blockchain::block block) {
	std::vector<std::string> keys = connected.keys();
	std::random_shuffle(keys.begin(), keys.end());
    for(std::string key : keys) {
    	auto it = connected.atMaybe(key);
    	if(it.first) {
    		try {
				it.second->sendBlock(block);
			} catch(const Peer::NetworkError& err) {
				log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer: " + std::string(err.what()));
			}
    	}
    }
}

double CryptoKernel::Network::syncProgress() {
	std::lock_guard<std::mutex> lock(heightMutex);
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
    for(const auto& peer : connectedStats.keys()) {
    	peerUrls.insert(peer);
    }

    return peerUrls;
}

uint64_t CryptoKernel::Network::getCurrentHeight() {
	std::lock_guard<std::mutex> lock(heightMutex);
    return currentHeight;
}

std::map<std::string, CryptoKernel::Network::peerStats>
CryptoKernel::Network::getPeerStats() {
    return connectedStats.copyMap();
}
