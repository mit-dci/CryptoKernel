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

Json::Value& CryptoKernel::Network::Connection::getInfo(Json::ArrayIndex& key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
}

Json::Value& CryptoKernel::Network::Connection::getInfo(std::string key) {
	std::lock_guard<std::mutex> im(infoMutex);
	return this->info[key];
}

void CryptoKernel::Network::Connection::setSendCipher(NoiseCipherState* cipher) {
	peer->setSendCipher(cipher);
}

void CryptoKernel::Network::Connection::setRecvCipher(NoiseCipherState* cipher) {
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
    bestHeight = 0;
    currentHeight = 0;
    encrypt = true;

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

    if(encrypt) {
    	log->printf(LOG_LEVEL_INFO, "Encryption enabled");
    	incomingEncryptionHandshakeThread.reset(new std::thread(&CryptoKernel::Network::incomingEncryptionHandshakeFunc, this));
    	outgoingEncryptionHandshakeThread.reset(new std::thread(&CryptoKernel::Network::outgoingEncryptionHandshakeFunc, this));
    }
}

CryptoKernel::Network::~Network() {
    running = false;
    connectionThread->join();
    networkThread->join();
    makeOutgoingConnectionsThread->join();
    infoOutgoingConnectionsThread->join();

    if(encrypt) {
    	incomingEncryptionHandshakeThread->join();
    	outgoingEncryptionHandshakeThread->join();
    }

    listener.close();
}

void CryptoKernel::Network::incomingEncryptionHandshakeFunc() {
	log->printf(LOG_LEVEL_INFO, "Network(): Incoming encryption handshake thread started");

	sf::TcpListener ls;
	if(ls.listen(port + 1) != sf::Socket::Done) {
		log->printf(LOG_LEVEL_ERR, "Network(): Could not bind to port " + std::to_string(port + 1));
	}

	selectorMutex.lock();
	selector.add(ls);
	selectorMutex.unlock();

	while(running)
	{
	    // Make the selector wait for data on any socket
	    if(selector.wait(sf::seconds(2)))
	    {
	    	log->printf(LOG_LEVEL_INFO, "Network(): Selector waiting for data on any socket...");
	        // Test the listener
	        if(selector.isReady(ls))
	        {
	            // The listener is ready: there is a pending connection
	            std::shared_ptr<sf::TcpSocket> client(new sf::TcpSocket);
	            client->setBlocking(false);
	            if(ls.accept(*client.get()) == sf::Socket::Done)
	            {
	            	log->printf(LOG_LEVEL_INFO, "Network(): Connection accepted from " + client->getRemoteAddress().toString());
	            	if(connected.contains(client->getRemoteAddress().toString())) {
	            		log->printf(LOG_LEVEL_INFO, "Network(): We are already conected to " + client->getRemoteAddress().toString());
	            		continue;
	            	}

	            	if(!handshakeClients.contains(client->getRemoteAddress().toString()) && !handshakeServers.contains(client->getRemoteAddress().toString())) {
	            		if(client->getRemoteAddress().toString() < myAddress.toString()) {
	            			log->printf(LOG_LEVEL_INFO, "Network(): Adding a noise client (to handshakeServers), " + client->getRemoteAddress().toString());
							// Add the new client to the clients list
							handshakeServers.at(client->getRemoteAddress().toString()).reset(new NoiseServer(client, 8888, log));
							// Add the new client to the selector so that we will
							// be notified when it sends something
							selectorMutex.lock();
							selector.add(*client.get());
							selectorMutex.unlock();
	            		}
	            		else {
	            			log->printf(LOG_LEVEL_INFO, "Creating new noise connection client at " + client->getRemoteAddress().toString());
							handshakeClients.at(client->getRemoteAddress().toString()).reset(new NoiseClient(client, client->getRemoteAddress().toString(), 88, log));
							log->printf(LOG_LEVEL_INFO, "Network(): Added connection to handshake clients: " + client->getRemoteAddress().toString());

							selectorMutex.lock();
							selector.add(*client.get());
							selectorMutex.unlock();
	            		}
	            	}

	            	else {
	            		log->printf(LOG_LEVEL_INFO, "Network(): We are fielding another request from " + client->getRemoteAddress().toString() + ".");
	            	}
	            }
	        }
	        else
	        {
	        	log->printf(LOG_LEVEL_INFO, "Network(): The listener is not ready");
	            // The listener socket is not ready, test all other sockets (the clients)
	        	std::vector<std::string> nccKeys = handshakeServers.keys(); // this should not contain any of the same things as handshakeClients
				for(std::string key : nccKeys) {
	                auto it = handshakeServers.find(key);
	                if(selector.isReady(*it->second->client.get())) {
	                	log->printf(LOG_LEVEL_INFO, "Network(): " + it->second->client->getRemoteAddress().toString() + " is ready with data.");
	                    // The client has sent some data, we can receive it
	                    sf::Packet packet;
	                    if(it->second->client->receive(packet) == sf::Socket::Done) {
	                    	it->second->receivePacket(packet);
	                    }
	                    else {
	                    	log->printf(LOG_LEVEL_INFO, "Network(): Something went wrong receiving packet from hs server "
	                    			+ it->first + ", disconnecting it.");
	                    	selectorMutex.lock();
	                    	selector.remove(*it->second->client.get());
	                    	selectorMutex.unlock();
	                    	handshakeServers.erase(it->first);
	                    }
	                }
	            }

	            log->printf(LOG_LEVEL_INFO, "Network(): About to loop through handshake client keys.");
	        	std::vector<std::string> ncsKeys = handshakeClients.keys(); // this should not contain any of the same things as handshakeServers
	        	for(std::string key : ncsKeys) {
	        		auto it = handshakeClients.find(key);
	        		if(selector.isReady(*it->second->server.get())) {
	        			log->printf(LOG_LEVEL_INFO, "Network(): " + key + " is ready with data.");
	        			sf::Packet packet;
	        			if(it->second->server->receive(packet) == sf::Socket::Done) {
	        				it->second->receivePacket(packet);
	        			}
	        			else {
	        				log->printf(LOG_LEVEL_INFO, "Network(): Something went wrong receiving packet from hs client "
	        						+ it->first + ", disconnecting it.");
	        				selectorMutex.lock();
							selector.remove(*it->second->server.get());
							selectorMutex.unlock();
							handshakeClients.erase(it->first);
	        			}
	        		}
	        	}
	        }
	    }
	    else {
	    	log->printf(LOG_LEVEL_INFO, "Network(): There's nothing on the selector, pausing.");
	    	std::this_thread::sleep_for(std::chrono::seconds(2));
	    }
	}

	ls.close();
}

void CryptoKernel::Network::outgoingEncryptionHandshakeFunc() {
	log->printf(LOG_LEVEL_INFO, "Network(): Outgoing encryption handshake started.");

	std::map<std::string, std::shared_ptr<sf::TcpSocket>> pendingConnections;

	while(running) {
		// let all of the encryption check ips know that I want to perform a handshake
		std::vector<std::string> addresses = peersToQuery.keys();
		std::random_shuffle(addresses.begin(), addresses.end());
		for(std::string addr : addresses) {
			std::shared_ptr<sf::TcpSocket> client(new sf::TcpSocket());
			//client->setBlocking(false); // Ultimately, it would be better to do this HERE instead of below, but it makes things more complicated
			log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + addr + " to query encryption preference.");
			if(client->connect(addr, port + 1, sf::seconds(3)) != sf::Socket::Done) {
				log->printf(LOG_LEVEL_INFO, "Network(): Couldn't query " + addr + " for encryption preference (it likely doesn't support encryption).");
				peersToQuery.erase(addr);
				plaintextHosts.insert(addr, true);
				continue;
			}
			client->setBlocking(false);
			log->printf(LOG_LEVEL_INFO, "Network(): Connection attempt to " + addr + " complete!");
			pendingConnections.insert(std::make_pair(addr, client));
			peersToQuery.erase(addr);
		}

		for(auto it = pendingConnections.begin(); it != pendingConnections.end();) {
			if(!handshakeClients.contains(it->first) && !handshakeServers.contains(it->first)) {
				if(it->first < myAddress.toString()) {
					log->printf(LOG_LEVEL_INFO, "Creating new noise connection server at " + it->first);
					handshakeServers.at(it->first).reset(new NoiseServer(it->second, 8888, log));
					log->printf(LOG_LEVEL_INFO, "Network(): Added connection to handshake servers: " + it->first);

					selectorMutex.lock();
					selector.add(*it->second.get());
					selectorMutex.unlock();
				}
				else {
					log->printf(LOG_LEVEL_INFO, "Creating new noise connection client at " + it->first);
					handshakeClients.at(it->first).reset(new NoiseClient(it->second, it->first, 88, log));
					log->printf(LOG_LEVEL_INFO, "Network(): Added connection to handshake clients: " + it->first);

					selectorMutex.lock();
					selector.add(*it->second.get());
					selectorMutex.unlock();
				}
			}
			pendingConnections.erase(it++);
		}

		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
}

void CryptoKernel::Network::makeOutgoingConnectionsWrapper() {
	while(running) {
		bool wait = false;
		makeOutgoingConnections(wait);
		if(wait) {
			std::this_thread::sleep_for(std::chrono::seconds(20)); // stop looking for a while
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
	}
}

void CryptoKernel::Network::infoOutgoingConnectionsWrapper() {
	while(running) {
		infoOutgoingConnections();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // just do this once every two seconds
	}
}

void CryptoKernel::Network::addConnection(sf::TcpSocket* socket, Json::Value& peerInfo, NoiseCipherState* send_cipher, NoiseCipherState* recv_cipher) {
	Connection* connection = new Connection;
	connection->setPeer(new Peer(socket, blockchain, this, false, log));

	peerInfo["lastseen"] = static_cast<uint64_t>(std::time(nullptr));
	peerInfo["score"] = 0;

	connection->setInfo(peerInfo);

	connection->setSendCipher(send_cipher);
	connection->setRecvCipher(recv_cipher);

	connected.at(socket->getRemoteAddress().toString()).reset(connection);
}

void CryptoKernel::Network::makeOutgoingConnections(bool& wait) {
	std::map<std::string, Json::Value> peersToTry;
	std::vector<std::string> peerIps;

	std::unique_ptr<Storage::Transaction> dbTx(networkdb->beginReadOnly());

	std::unique_ptr<Storage::Table::Iterator> it(new Storage::Table::Iterator(peers.get(), networkdb.get(), dbTx->snapshot));

	for(it->SeekToFirst(); it->Valid(); it->Next()) {
		if(connected.size() >= 8) { // honestly, this is enough
			log->printf(LOG_LEVEL_INFO, "Network(): We already have 8 connections, skipping " + it->key());
			wait = true;
			it.reset();
			return;
		}

		Json::Value peerInfo = it->value();

		if(connected.contains(it->key()) || peersToQuery.contains(it->key())) {
			continue;
		}

		std::time_t result = std::time(nullptr);

		const auto banIt = banned.find(it->key());
		if(banIt != banned.end()) {
			if(banIt->second > static_cast<uint64_t>(result)) {
				log->printf(LOG_LEVEL_INFO, "Network(): " + it->key() + " is banned!");
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

		if(handshakeClients.contains(it->key())) {
			auto entry = handshakeClients.find(it->key());
			if(entry->second->getHandshakeSuccess()) {
				log->printf(LOG_LEVEL_INFO, "Network(): Client handshake complete, " + entry->first);
				if(sockets.contains(it->key())) {
					sf::TcpSocket* socket = sockets.find(it->key())->second;
					if(socket) {
						addConnection(socket, peerInfo, entry->second->send_cipher, entry->second->recv_cipher);
						selector.remove(*entry->second->server);
						handshakeClients.erase(it->key());
					}
					else {
						log->printf(LOG_LEVEL_INFO, "Network(): Socket for " + it->key() + " not found.");
					}
				}
			}
			else if(entry->second->getHandshakeComplete() && !entry->second->getHandshakeSuccess()) {
				log->printf(LOG_LEVEL_INFO, "Network(): Client, the noise handshake for " + it->key() + " failed.");

				selector.remove(*entry->second->server);
				handshakeClients.erase(it->key());
				peersToQuery.erase(it->key());

			}
			continue;
		}
		else if(handshakeServers.contains(it->key())) {
			auto entry = handshakeServers.find(it->key());
			if(entry->second->getHandshakeSuccess()) {
				if(sockets.contains(it->key())) {
					log->printf(LOG_LEVEL_INFO, "Network(): Server handshake complete, " + entry->first);
					sf::TcpSocket* socket = sockets.find(it->key())->second;
					if(socket) {
						addConnection(socket, peerInfo, entry->second->sendCipher, entry->second->recvCipher);
						selector.remove(*entry->second->client);
						handshakeServers.erase(it->key());
					}
					else {
						log->printf(LOG_LEVEL_INFO, "Network(): Socket for " + it->key() + " not found.");
					}
				}
			}
			else if(entry->second->getHandshakeComplete() && !entry->second->getHandshakeSuccess()) {
				log->printf(LOG_LEVEL_INFO, "SERVER The handshake for " + it->key() + " failed.");

				selector.remove(*entry->second->client);
				handshakeServers.erase(it->key());
				peersToQuery.erase(it->key());
			}
			continue;
		}
		else if(plaintextHosts.contains(it->key())) { // connect over plaintext
			if(sockets.contains(it->key())) {
				log->printf(LOG_LEVEL_INFO, "Network(): Making an unecrypted connection to " + it->key());
				sf::TcpSocket* socket = sockets.find(it->key())->second;
				if(socket) {
					addConnection(socket, peerInfo);
				}
				else {
					log->printf(LOG_LEVEL_INFO, "Network(): Socket for " + it->key() + " not found.");
				}
			}
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

			peersToQuery.insert(peerIp, true); // really, we just need a set here, not a map
			sockets.insert(peerIp, socket);
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
						throw Peer::NetworkError("peer has an incompatible major version");
					}

					const auto banIt = banned.find(it->first);
					if(banIt != banned.end()) {
						if(banIt->second > static_cast<uint64_t>(std::time(nullptr))) {
							log->printf(LOG_LEVEL_WARN,
										"Network(): Disconnecting " + it->first + " for being banned");
							throw Peer::NetworkError("peer is banned");
						}
					}

                    it->second->setInfo("version", info["version"].asString());
					it->second->setInfo("height", info["tipHeight"].asUInt64());

					// update connected stats
					peerStats stats = it->second->getPeerStats();
					stats.version = it->second->getInfo("version").asString();
					stats.blockHeight = it->second->getInfo("height").asUInt64();
					connectedStats.insert(std::make_pair(it->first, stats));

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
							throw Peer::NetworkError("peer sent a malformed peer IP address");
						}
					}
				} catch(const Json::Exception& e) {
					changeScore(it->first, 50);
					throw Peer::NetworkError("peer sent a malformed info message");
				}

				const std::time_t result = std::time(nullptr);
				it->second->setInfo("lastseen", static_cast<uint64_t>(result));
			} catch(const Peer::NetworkError& e) {
				log->printf(LOG_LEVEL_WARN,
							"Network(): Failed to contact " + it->first + ", disconnecting it for: " + e.what());

				peers->put(dbTx.get(), it->first, it->second->getCachedInfo());
				connectedStats.erase(it->first);
				handshakeServers.erase(it->first);
				handshakeClients.erase(it->first);

				sockets.erase(it->first);

				peersToQuery.erase(it->first);
				it->second->release();
				connected.erase(it);
				continue;
			}
			it->second->release();
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
                defer d([&]{it->second->release();});
        		if(it->second->getInfo("height").asUInt64() > bestHeight) {
					bestHeight = it->second->getInfo("height").asUInt64();
				}
        	}
        }

        if(this->currentHeight > bestHeight) {
            bestHeight = this->currentHeight;
        }
        this->bestHeight = bestHeight;

        log->printf(LOG_LEVEL_INFO,
                    "Network(): Current height: " + std::to_string(currentHeight) + ", best height: " +
                    std::to_string(bestHeight) + ", start height: " + std::to_string(startHeight));

        bool madeProgress = false;

        //Detect if we are behind
        if(bestHeight > currentHeight) {
            keys = connected.keys();
            std::random_shuffle(keys.begin(), keys.end());
			for(auto key : keys) {
				auto it = connected.find(key);
				if(it != connected.end() && it->second->acquire()) {
                    defer d([&]{it->second->release();});
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
								} catch(const Peer::NetworkError& e) {
									log->printf(LOG_LEVEL_WARN,
												"Network(): Failed to contact " + it->first + " " + e.what() +
												" while downloading blocks");
									break;
								}

								log->printf(LOG_LEVEL_INFO, "Network(): Testing if we have block " + std::to_string(blocks.rbegin()->getHeight() - 1));

								try {
									blockchain->getBlockDB(blocks.rbegin()->getPreviousBlockId().toString());
								} catch(const CryptoKernel::Blockchain::NotFoundException& e) {
									if(currentHeight == 1) {
										// This peer has a different genesis block to us
										changeScore(it->first, 250);
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
							} catch(const Peer::NetworkError& e) {
								log->printf(LOG_LEVEL_WARN,
											"Network(): Failed to contact " + it->first + " " + e.what() +
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
								this->currentHeight = currentHeight;
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
        sf::SocketSelector selector;
        selector.add(listener);
        sf::TcpSocket* client = new sf::TcpSocket();
        if(selector.wait(sf::seconds(2))) {
            if(listener.accept(*client) != sf::Socket::Done) {
                delete client;
                continue;
            }

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

            // the connection code that was here previously is now handled in makeOutgoingConnections
            sockets.insert(client->getRemoteAddress().toString(), client);

            std::unique_ptr<Storage::Transaction> dbTx(networkdb->begin());
            if(!peers->get(dbTx.get(), client->getRemoteAddress().toString()).isObject()) {
            	log->printf(LOG_LEVEL_INFO, client->getRemoteAddress().toString() + ", which just connected, is new to us!");
				Json::Value newSeed;
				newSeed["lastseen"] = 0;
				newSeed["height"] = 1;
				newSeed["score"] = 0;
				peers->put(dbTx.get(), client->getRemoteAddress().toString(), newSeed);
				dbTx->commit();
			}
            else {
            	// todo... something
            }

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
		auto it = connected.find(key);
		if(it != connected.end() && it->second->acquire()) {
            defer d([&]{it->second->release();});
			try {
				it->second->sendTransactions(transactions);
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
    	auto it = connected.find(key);
    	if(it != connected.end() && it->second->acquire()) {
            defer d([&]{it->second->release();});
    		try {
				it->second->sendBlock(block);
			} catch(const Peer::NetworkError& err) {
				log->printf(LOG_LEVEL_WARN, "Network::broadcastBlock(): Failed to contact peer: " + std::string(err.what()));
			}
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
    for(const auto& peer : connected.keys()) {
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
