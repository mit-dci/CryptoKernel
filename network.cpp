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

#include <sstream>

#include "network.h"
#include "version.h"

CryptoKernel::Network::Network(Log *GlobalLog)
{
    status = true;
    connections = 0;
    log = GlobalLog;
    log->printf(LOG_LEVEL_INFO, "Initialising network");
    if(enet_initialize() != 0)
    {
        log->printf(LOG_LEVEL_ERR, "Initialisation failed");
        status = false;
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "Initialisation successful");

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = 49000;

        log->printf(LOG_LEVEL_INFO, "Creating server");
        server = enet_host_create(&address, 125, 2, 0, 0);

        if(server == NULL)
        {
            log->printf(LOG_LEVEL_WARN, "Error creating server");
        }
        else
        {
            log->printf(LOG_LEVEL_INFO, "Server created successfully");
        }

        log->printf(LOG_LEVEL_INFO, "Creating client");
        client = enet_host_create(NULL, 8, 2, 256000, 256000);
        if(client == NULL)
        {
            log->printf(LOG_LEVEL_ERR, "Error creating client");
            status = false;
        }
        else
        {
            log->printf(LOG_LEVEL_INFO, "Client created successfully");
            eventThread = new std::thread(&CryptoKernel::Network::HandleEvents, this);
            connectionsThread = new std::thread(&CryptoKernel::Network::HandleConnections, this);
        }
    }
}

CryptoKernel::Network::~Network()
{
    delete eventThread;
    delete connectionsThread;

    serverMutex.lock();
    if(server != NULL)
    {
        log->printf(LOG_LEVEL_INFO, "Destroying server");
        enet_host_destroy(server);
    }
    serverMutex.unlock();

    clientMutex.lock();
    if(client != NULL)
    {
        log->printf(LOG_LEVEL_INFO, "Destroying client");
        enet_host_destroy(client);
    }
    clientMutex.unlock();

    log->printf(LOG_LEVEL_INFO, "Deinitialising network");
    enet_deinitialize();
}

bool CryptoKernel::Network::getStatus()
{
    return status;
}

void CryptoKernel::Network::HandleEvents()
{
    while(true)
    {
        serverMutex.lock();
        if(server != NULL)
        {
            ENetEvent event;
            while(enet_host_service(server, &event, 1000) > 0)
            {
                switch(event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    std::stringstream location;
                    location << uintToAddress(event.peer->address.host) << ":" << event.peer->address.port;
                    log->printf(LOG_LEVEL_INFO, "A new client connected from " + location.str());

                    log->printf(LOG_LEVEL_INFO, "Asking for peer information from " + location.str());
                    ENetPacket *packet = enet_packet_create("sendinfo=>", 11, ENET_PACKET_FLAG_RELIABLE);

                    enet_peer_send(event.peer, 0, packet);
                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                    HandlePacket(&event);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    std::stringstream location;
                    location << uintToAddress(event.peer->address.host) << ":" << event.peer->address.port;
                    log->printf(LOG_LEVEL_INFO, "Client disconnected from " + location.str());
                    connectionsMutex.lock();
                    connections--;
                    connectionsMutex.unlock();
                    enet_peer_reset(event.peer);
                    break;
                }

                case ENET_EVENT_TYPE_NONE:
                    break;
                }
            }
        }
        serverMutex.unlock();

        clientMutex.lock();
        ENetEvent event;
        while(enet_host_service(client, &event, 1000) > 0)
        {
            switch(event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                HandlePacket(&event);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                std::stringstream location;
                location << uintToAddress(event.peer->address.host) << ":" << event.peer->address.port;
                log->printf(LOG_LEVEL_INFO, "Client disconnected from " + location.str());
                connectionsMutex.lock();
                connections--;
                connectionsMutex.unlock();
                enet_peer_reset(event.peer);
                break;
            }

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
        clientMutex.unlock();
    }
}

void CryptoKernel::Network::HandlePacket(ENetEvent *event)
{
    std::string datastring(reinterpret_cast<char const*>(event->packet->data), event->packet->dataLength);
    std::string request = datastring.substr(0, datastring.find("=>"));
    if(request == "sendinfo")
    {
        std::stringstream location;
        location << uintToAddress(event->peer->address.host) << ":" << event->peer->address.port;
        log->printf(LOG_LEVEL_INFO, "Sending version information to " + location.str());
        std::stringstream packetstring;
        packetstring << "info=>" << version;
        ENetPacket *packet = enet_packet_create(packetstring.str().c_str(), packetstring.str().length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(event->peer, 0, packet);
    }
    else if(request == "info")
    {
        std::string info = datastring.substr(datastring.find("=>") + 2);
        std::stringstream location;
        location << uintToAddress(event->peer->address.host) << ":" << event->peer->address.port;
        log->printf(LOG_LEVEL_INFO, "Received version information from " + location.str() + ": " + info);

        connectionsMutex.lock();
        connections++;
        connectionsMutex.unlock();

        if(info.substr(0, info.find(".")) != version.substr(0, version.find(".")))
        {
            log->printf(LOG_LEVEL_INFO, "Peer has a different major version than us, disconnecting it");
            enet_peer_disconnect(event->peer, 0);
        }
    }
    else if(request == "message")
    {
        std::stringstream location;
        location << uintToAddress(event->peer->address.host) << ":" << event->peer->address.port;
        log->printf(LOG_LEVEL_INFO, "Received message from " + location.str() + ". Adding it to the message queue");
        std::string message = datastring.substr(datastring.find("=>") + 2);
        queueMutex.lock();
        messageQueue.push(message);
        queueMutex.unlock();
    }
    else
    {
        log->printf(LOG_LEVEL_INFO, "Peer misbehaving, disconnecting it");
        enet_peer_disconnect(event->peer, 0);
    }
}

void CryptoKernel::Network::HandleConnections()
{
    while(true)
    {
        connectionsMutex.lock();
        if(connections < 8)
        {
            connectionsMutex.unlock();
            std::ifstream peersfile("peers.txt");
            if(peersfile.is_open())
            {
                std::string line;
                while(std::getline(peersfile, line) && connections < 8)
                {
                    connectPeer(line);
                }
                peersfile.close();
                break;
            }
            else
            {
                log->printf(LOG_LEVEL_ERR, "Could not open peers list");
                break;
            }
        }
        connectionsMutex.unlock();
    }
}

bool CryptoKernel::Network::connectPeer(std::string peeraddress)
{
    ENetAddress address;
    ENetEvent event;
    ENetPeer *peer;
    enet_address_set_host(&address, peeraddress.c_str());
    address.port = 49000;

    log->printf(LOG_LEVEL_INFO, "Attempting to connect to " + peeraddress);

    clientMutex.lock();
    peer = enet_host_connect(client, &address, 2, 0);

    if(peer == NULL)
    {
        clientMutex.unlock();
        log->printf(LOG_LEVEL_WARN, "There are no available peers for connection");
        return false;
    }
    else
    {
        if(enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
        {
            log->printf(LOG_LEVEL_INFO, "Connection successful");
            log->printf(LOG_LEVEL_INFO, "Asking for peer information from " + peeraddress);
            ENetPacket *packet = enet_packet_create("sendinfo=>", 11, ENET_PACKET_FLAG_RELIABLE);

            enet_peer_send(peer, 0, packet);
            clientMutex.unlock();
            return true;
        }
        else
        {
            enet_peer_reset(peer);
            clientMutex.unlock();
            log->printf(LOG_LEVEL_INFO, "Connection timed out");
            return false;
        }
    }
}

bool CryptoKernel::Network::sendMessage(std::string message)
{
    connectionsMutex.lock();
    if(connections > 0 && status && message.length() > 0)
    {
        connectionsMutex.unlock();
        log->printf(LOG_LEVEL_INFO, "Broadcasting message on network");
        std::stringstream packetstaging;
        packetstaging << "message=>" << message;

        if(client != NULL)
        {
            ENetPacket *packet = enet_packet_create(packetstaging.str().c_str(), packetstaging.str().length(), ENET_PACKET_FLAG_RELIABLE);

            clientMutex.lock();
            enet_host_broadcast(client, 0, packet);
            clientMutex.unlock();
        }

        if(server != NULL)
        {
            ENetPacket *packet2 = enet_packet_create(packetstaging.str().c_str(), packetstaging.str().length(), ENET_PACKET_FLAG_RELIABLE);

            serverMutex.lock();
            enet_host_broadcast(server, 0, packet2);
            serverMutex.unlock();
        }

        return true;
    }
    else
    {
        connectionsMutex.unlock();
        return false;
    }
}

std::string CryptoKernel::Network::popMessage()
{
    queueMutex.lock();
    if(status && messageQueue.size() > 0)
    {
        std::string message = messageQueue.front();
        messageQueue.pop();
        queueMutex.unlock();
        return message;
    }
    else
    {
        queueMutex.unlock();
        return "";
    }
}

int CryptoKernel::Network::getConnections()
{
    connectionsMutex.lock();
    int temp = connections;
    connectionsMutex.unlock();

    return temp;
}

std::string CryptoKernel::Network::uintToAddress(uint32_t address)
{
    uint8_t bytes[4];
    bytes[0] = address & 0xFF;
    bytes[1] = (address >> 8) & 0xFF;
    bytes[2] = (address >> 16) & 0xFF;
    bytes[3] = (address >> 24) & 0xFF;

    std::stringstream buffer;

    buffer << (unsigned int)bytes[0] << "." << (unsigned int)bytes[1] << "." << (unsigned int)bytes[2] << "." << (unsigned int)bytes[3];

    return buffer.str();
}
