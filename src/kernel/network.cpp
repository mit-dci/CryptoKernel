#include "network.h"
#include "networkserver.h"
#include "networkclient.h"

CryptoKernel::Network::Network(CryptoKernel::Blockchain* blockchain)
{
    httpServer.reset(new jsonrpc::HttpServer(49000));
    server.reset(new Server(*httpServer.get()));
    server->setBlockchain(blockchain);
    server->StartListening();

    peers = new CryptoKernel::Storage("./peers");

    Json::Value seeds = peers->get("seeds");
    if(seeds.size() <= 0)
    {
        std::ifstream infile("peers.txt");
        if(!infile.is_open())
        {
            throw std::runtime_error("Could not open peers file");
        }

        std::string line;
        while(std::getline(infile, line))
        {
            Json::Value newSeed;
            newSeed["url"] = line;
            newSeed["lastseen"] = -1;
            newSeed["height"] = 1;
            seeds.append(newSeed);
        }

        infile.close();

        peers->store("seeds", seeds);
    }

    if(seeds.size() <= 0)
    {
        throw std::runtime_error("There are no known peers to connect to");
    }

    for(unsigned int i = 0; i < seeds.size(); i++)
    {
        // Attempt to connect to peer
        Client client(seeds[i]["url"].asString());
        // Get height
        Json::Value info = client.getInfo();

        // Update info
        seeds[i]["height"] = info["tipHeight"];

        std::time_t result = std::time(nullptr);
        seeds[i]["lastseen"] = std::asctime(std::localtime(&result));;
    }

    peers->store("seeds", seeds);
}

CryptoKernel::Network::~Network()
{
    server->StopListening();
    delete peers;
}
