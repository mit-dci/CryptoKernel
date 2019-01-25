#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <memory>
#include <thread>
#include <functional>

#include <SFML/Network.hpp>

#include "blockchain.h"
#include "concurrentmap.h"
#include "NoiseServer.h"
#include "NoiseClient.h"

namespace CryptoKernel {
/**
* This class provides a peer-to-peer network between multiple blockchains
*/
class Network {
public:
    /**
    * Constructs a network object with the given log and blockchain. Attempts
    * to connect to saved seeds or those specified in peers.txt. Causes the
    * program to listen for incoming connections on port 49000.
    *
    * @param log a pointer to the CK log to use
    * @param blockchain a pointer to the blockchain to sync
    * @param port the port to listen on
    * @param dbDir the directory of the peers database
    */
    Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain,
            const unsigned int port, const std::string& dbDir);

    /**
    * Default destructor
    */
    ~Network();

    /**
    * Returns the number of currently connected peers
    *
    * @return an unsigned integer with the number of connections
    */
    unsigned int getConnections();

    /**
    * Broadcast a set of transactions to connected peers
    *
    * @param transactions the transactions to broadcast
    */
    void broadcastTransactions(const std::vector<CryptoKernel::Blockchain::transaction>
                               transactions);

    /**
    * Broadcast a block to connected peers
    *
    * @param block the block to broadcast
    */
    void broadcastBlock(const CryptoKernel::Blockchain::block block);

    /**
    * Returns an estimate of synchronisation progress
    *
    * @return a double representing the progress of synchronisation out of 1.0
    */
    double syncProgress();

    /**
    * Returns the set of peer URLs the node is currently connected to
    *
    * @return a set of strings representing connected node URLs
    */
    std::set<std::string> getConnectedPeers();

    /**
    * Returns the current sync height of this network's blockchain
    *
    * @return the current height of the main chain of the blockchain
    */
    uint64_t getCurrentHeight();

    struct peerStats {
        unsigned int ping;
        bool incoming;
        bool encrypted;
        uint64_t connectedSince;
        uint64_t transferUp;
        uint64_t transferDown;
        std::string version;
        uint64_t blockHeight;
    };

    /**
     * Returns a map of peers and their related stats
     *
     * @return a map where the keys are the peer IP address and the values
     * are peerStats structs containing information about the peer
     */
     std::map<std::string, peerStats> getPeerStats();

private:
    class Peer;

    void changeScore(const std::string& url, const uint64_t score);

    class Connection {
    public:
    	Connection();

    	Json::Value getInfo();
		void sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction>& transactions);
		void sendBlock(const CryptoKernel::Blockchain::block& block);
		std::vector<CryptoKernel::Blockchain::transaction> getUnconfirmedTransactions();
		CryptoKernel::Blockchain::block getBlock(const uint64_t height, const std::string& id);
		std::vector<CryptoKernel::Blockchain::block> getBlocks(const uint64_t start, const uint64_t end);
        CryptoKernel::Network::peerStats getPeerStats();

		void setPeer(Peer* peer);
		void setInfo(Json::ArrayIndex& key, Json::Value& value);
		void setInfo(std::string key, uint64_t value);
		void setInfo(std::string key, std::string value);
		void setInfo(Json::Value info);
		Json::Value getInfo(Json::ArrayIndex& key);
		Json::Value getInfo(std::string key);
		Json::Value getCachedInfo();
		Network::peerStats getPeerStats() const;

        void setSendCipher(NoiseCipherState* cipher);
		void setRecvCipher(NoiseCipherState* cipher);

    	~Connection();

    private:
    	std::unique_ptr<Peer> peer;
		Json::Value info;
		std::mutex peerMutex;
		std::mutex modMutex;
		std::mutex infoMutex;
        
        bool acquire();
		void release();
	};

    ConcurrentMap<std::string, std::shared_ptr<Connection>> connected;
    std::recursive_mutex connectedMutex;

    ConcurrentMap<std::string, peerStats> connectedStats;

    CryptoKernel::Log* log;
    CryptoKernel::Blockchain* blockchain;

    std::unique_ptr<CryptoKernel::Storage> networkdb;
    std::unique_ptr<Storage::Table> peers;

    bool running;

    void networkFunc();
    std::unique_ptr<std::thread> networkThread;

    void connectionFunc();
	std::unique_ptr<std::thread> connectionThread;

	void makeOutgoingConnections(bool& wait);
	void makeOutgoingConnectionsWrapper();
    std::unique_ptr<std::thread> makeOutgoingConnectionsThread;

    void infoOutgoingConnections();
    void infoOutgoingConnectionsWrapper();
    std::unique_ptr<std::thread> infoOutgoingConnectionsThread;

    void incomingEncryptionHandshakeWrapper();
    void incomingEncryptionHandshakeFunc(sf::TcpListener& ls, sf::SocketSelector& selector);
    std::unique_ptr<std::thread> incomingEncryptionHandshakeThread;

    void outgoingEncryptionHandshakeWrapper();
    void outgoingEncryptionHandshakeFunc();
	std::unique_ptr<std::thread> outgoingEncryptionHandshakeThread;

    void addToNoisePool(sf::TcpSocket* socket);

    void postHandshakeConnect();
    std::unique_ptr<std::thread> postHandshakeConnectThread;

    std::mutex handshakeMutex;
    ConcurrentMap<std::string, std::shared_ptr<NoiseClient>> handshakeClients;
    ConcurrentMap<std::string, std::shared_ptr<NoiseServer>> handshakeServers;
    ConcurrentMap<std::string, bool> plaintextHosts;
    ConcurrentMap<std::string, bool> peersToQuery; // (regarding encyrption preference)
    ConcurrentMap<std::string, std::shared_ptr<Connection>> connectedPending; // connections we've made but aren't yet ready to use
    sf::TcpListener listener;

    ConcurrentMap<std::string, uint64_t> banned;

    void transferConnection(std::string addr, NoiseCipherState* send_cipher=0, NoiseCipherState* recv_cipher=0);
    
    sf::IpAddress myAddress;

    std::mutex heightMutex;
    uint64_t bestHeight;
    uint64_t currentHeight;

    unsigned int port;
};

class defer {
    public:
        defer(std::function<void()> &&f) : f(f) {}
        ~defer() { f(); }
    private:
        std::function<void()> f;
};

}

#endif // NETWORK_H_INCLUDED
