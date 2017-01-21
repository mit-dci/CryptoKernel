#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <memory>
#include <thread>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include "blockchain.h"

namespace CryptoKernel
{
    class Network
    {
        public:
            Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain);
            ~Network();
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
