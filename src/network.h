/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2016  James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <enet/enet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>

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
        std::queue<std::string> messageQueue;
        std::mutex queueMutex;
        std::mutex connectionsMutex;
        std::mutex serverMutex;
        std::mutex clientMutex;
        std::thread *eventThread;
        std::thread *connectionsThread;
        void HandleEvents();
        void HandlePacket(ENetEvent *event);
        void HandleConnections();
        std::string uintToAddress(uint32_t address);
};

}

#endif // NETWORK_H_INCLUDED
