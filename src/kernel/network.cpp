#include "network.h"
#include "networkserver.h"

CryptoKernel::Network::Network(CryptoKernel::Blockchain* blockchain)
{
    httpServer.reset(new jsonrpc::HttpServer(49000));
    server.reset(new Server(*httpServer.get()));
    server->setBlockchain(blockchain);
    server->StartListening();
}

CryptoKernel::Network::~Network()
{
    server->StopListening();
}
