#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <enet/enet.h>
#include <thread>
#include <mutex>
#include <queue>

#include "log.h"

namespace CryptoKernel
{

class Network
{
    public:
        Network(Log *GlobalLog);
        ~Network();
        bool getStatus();
        bool connectPeer(std::string peeraddress);
        bool sendMessage(std::string message);
        std::string popMessage();
        int getConnections();

    private:
        Log *log;
        ENetHost *server;
        ENetHost *client;
        bool status;
        int connections;
        std::queue <std::string>messageQueue;
        std::mutex queueMutex;
        std::mutex connectionsMutex;
        std::mutex serverMutex;
        std::mutex clientMutex;
        std::thread *eventThread;
        std::thread *connectionsThread;
        void HandleEvents();
        void HandlePacket(ENetEvent *event);
        void HandleConnections();
};

}

#endif // NETWORK_H_INCLUDED
