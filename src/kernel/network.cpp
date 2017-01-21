#include "network.h"
#include "networkserver.h"
#include "networkclient.h"

CryptoKernel::Network::Network(CryptoKernel::Log* log, CryptoKernel::Blockchain* blockchain)
{
    this->log = log;

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

    running = true;

    // Start management thread
    networkThread.reset(new std::thread(&CryptoKernel::Network::networkFunc, this));
}

CryptoKernel::Network::~Network()
{
    running = false;
    networkThread->join();
    server->StopListening();
    delete peers;
}

void CryptoKernel::Network::networkFunc()
{
    while(running)
    {
        Json::Value seeds = peers->get("seeds");

        if(connected.size() < 8)
        {
            for(unsigned int i = 0; i < seeds.size(); i++)
            {
                if(connected.size() >= 8)
                {
                    break;
                }
                // Attempt to connect to peer
                Client client(seeds[i]["url"].asString());
                // Get height
                Json::Value info;
                try
                {
                    log->printf(LOG_LEVEL_INFO, "Network(): Attempting to connect to " + seeds[i]["url"].asString());
                    info = client.getInfo();
                }
                catch(jsonrpc::JsonRpcException& e)
                {
                    log->printf(LOG_LEVEL_WARN, "Network(): Failed to connect to " + seeds[i]["url"].asString());
                    continue;
                }

                log->printf(LOG_LEVEL_INFO, "Network(): Successfully connected to " + seeds[i]["url"].asString());

                // Update info
                seeds[i]["height"] = info["tipHeight"];

                std::time_t result = std::time(nullptr);
                seeds[i]["lastseen"] = std::asctime(std::localtime(&result));

                connected[seeds[i]["url"].asString()] = seeds[i];
            }
        }

        peers->store("seeds", seeds);

        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
}

unsigned int CryptoKernel::Network::getConnections()
{
    return connected.size();
}
