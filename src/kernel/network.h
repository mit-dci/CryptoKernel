#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <memory>
#include <thread>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include "blockchain.h"

namespace CryptoKernel
{
    /**
    * This class provides a peer-to-peer network between multiple blockchains
    */
    class Network
    {
        public:
            /**
            * Constructs a network object with the given log and blockchain. Attempts
            * to connect to saved seeds or those specified in peers.txt. Causes the
            * program to listen for incoming connections on port 49000.
            *
            * @param log a pointer to the CK log to use
            * @param blockchain a pointer to the blockchain to sync
            */
            Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain);

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
            void broadcastTransactions(const std::vector<CryptoKernel::Blockchain::transaction> transactions);

        private:
            class Server;
            class Client;

            std::unique_ptr<jsonrpc::HttpServer> httpServer;
            std::unique_ptr<Server> server;

            CryptoKernel::Storage* peers;

            std::unique_ptr<std::thread> networkThread;

            void networkFunc();
            bool running;

            struct Peer
            {
                Json::Value info;
                std::unique_ptr<Client> client;
            };

            std::map<std::string, Peer*> connected;

            CryptoKernel::Log* log;
            CryptoKernel::Blockchain* blockchain;
    };
}

#endif // NETWORK_H_INCLUDED
