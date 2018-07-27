#ifndef NETWORKPEER_H_INCLUDED
#define NETWORKPEER_H_INCLUDED

#include <random>

#include <SFML/Network.hpp>
#include <condition_variable>

#include "network.h"

class CryptoKernel::Network::Peer {
public:
    Peer(sf::TcpSocket* client, CryptoKernel::Blockchain* blockchain,
         CryptoKernel::Network* network, const bool incoming);
    ~Peer();

    Json::Value getInfo();
    void sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction>& 
                          transactions);
    void sendBlock(const CryptoKernel::Blockchain::block& block);
    std::vector<CryptoKernel::Blockchain::transaction> getUnconfirmedTransactions();
    CryptoKernel::Blockchain::block getBlock(const uint64_t height, const std::string& id);
    std::vector<CryptoKernel::Blockchain::block> getBlocks(const uint64_t start,
                                                           const uint64_t end);
    
    Network::peerStats getPeerStats() const;

    sf::Socket::Status sendPacket(std::string& data);
	sf::Socket::Status receivePacket(sf::Packet** packet);

    class NetworkError : std::exception {
    public:
        NetworkError(const std::string& err) {
            msg = err;
        }

        virtual const char* what() const throw() {
            return msg.c_str();
        }
    private:
        std::string msg;
    };

private:
    sf::TcpSocket* client;
    CryptoKernel::Blockchain* blockchain;
    CryptoKernel::Network* network;
    std::mutex clientMutex;
    std::condition_variable responseReady;
    Json::Value sendRecv(const Json::Value& request);
    void send(const Json::Value& response);
    void requestFunc();
    bool running;
    std::unique_ptr<std::thread> requestThread;

    std::map<uint64_t, Json::Value> responses;

    std::map<uint64_t, bool> requests;

    std::default_random_engine generator;
    
    Network::peerStats stats;

    NoiseCipherState* send_cipher = 0;
	NoiseCipherState* recv_cipher = 0;
};

#endif // NETWORKPEER_H_INCLUDED
