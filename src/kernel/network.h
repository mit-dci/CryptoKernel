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

        private:
            class Server;
            class Client;

            std::unique_ptr<jsonrpc::HttpServer> httpServer;
            std::unique_ptr<Server> server;

            CryptoKernel::Storage* peers;

            std::unique_ptr<std::thread> networkThread;

            void networkFunc();
            bool running;

            std::map<std::string, Json::Value> connected;

            CryptoKernel::Log* log;
    };
}

#endif // NETWORK_H_INCLUDED
