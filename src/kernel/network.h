#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <memory>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include "blockchain.h"

namespace CryptoKernel
{
    class Network
    {
        public:
            Network(CryptoKernel::Blockchain* blockchain);
            ~Network();

        private:
            class Server;
            class Client;

            std::unique_ptr<jsonrpc::HttpServer> httpServer;
            std::unique_ptr<Server> server;
    };
}

#endif // NETWORK_H_INCLUDED
