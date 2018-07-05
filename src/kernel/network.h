#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <memory>
#include <thread>

#include <SFML/Network.hpp>

#include "blockchain.h"

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

    struct PeerInfo {
        std::unique_ptr<Peer> peer;
        Json::Value info;
    };
    std::map<std::string, std::unique_ptr<PeerInfo>> connected;
    std::recursive_mutex connectedMutex;

    std::map<std::string, peerStats> connectedStats;
    std::mutex connectedStatsMutex;

    CryptoKernel::Log* log;
    CryptoKernel::Blockchain* blockchain;

    std::unique_ptr<CryptoKernel::Storage> networkdb;
    std::unique_ptr<Storage::Table> peers;

    bool running;

    void networkFunc();
    std::unique_ptr<std::thread> networkThread;

    void connectionFunc();
    std::unique_ptr<std::thread> connectionThread;

    void peerFunc();
    std::unique_ptr<std::thread> peerThread;

    sf::TcpListener listener;

    std::map<std::string, uint64_t> banned;

    sf::IpAddress myAddress;

    uint64_t bestHeight;
    uint64_t currentHeight;

    unsigned int port;
};
}

#endif // NETWORK_H_INCLUDED
