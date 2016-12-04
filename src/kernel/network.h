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

#include <thread>
#include <vector>
#include <mutex>
#include <random>

#include <SFML/Network.hpp>

#include "log.h"
#include "blockchain.h"

namespace CryptoKernel
{

/**
* Peer-to-peer networking class for CryptoKernel. Handles the protocol
* between peers and maintaining connections between peers. Provides methods
* for getting a block or a set of blocks and broadcasting transactions and blocks.
* Automatically keeps tracks of the status of each of the peers with key information
* like which peers are currently considered to be on the longest chain.
*/
class Network
{
    public:
        /**
        * Constructs an instance of Network. Sets up a server listening on port 49000
        * and a client for connecting to other peers. Writes to the given log and uses
        * the given blockchain for data
        *
        * @param GlobalLog a pointer to the log this Network should use
        * @param blockchain a pointer to the blockchain to use
        */
        Network(Log *log, Blockchain *blockchain);

        /**
        * Default destructor
        */
        ~Network();

        /**
        * Attempts to connect to the given peer
        *
        * @param peerAddress a string containing the IPv4 address of the peer to connect to
        * @return true iff the connection was successful, false otherwise
        */
        bool connectPeer(const std::string peerAddress);

        /**
        * Sends the given transaction to all connected peers
        *
        * @param tx a valid transaction to broadcast
        */
        void sendTransaction(const CryptoKernel::Blockchain::transaction tx);

        /**
        * Sends the given block to all connected peers
        *
        * @param block a valid block to broadcast
        */
        void sendBlock(const CryptoKernel::Blockchain::block block);

        /**
        * Returns the number of currently connected peers
        *
        * @return the number of connected peers
        */
        unsigned int getConnections();

        /**
        * A blocking function which retrieves a vector of blocks starting at the given
        * block id and ending at either the tip of the senders chain or 500 blocks forward.
        * Attempts to retrieve blocks from a random peer which is considered to be on the
        * longest chain. Blocks for up to 30 seconds before returning an empty vector.
        *
        * @param id the id of the first block in the set to retrieve
        * @return a vector containing the blocks in order from first to last. If the function
        *         times out, return an empty vector
        */
        std::vector<CryptoKernel::Blockchain::block> getBlocks(const std::string id);

        /**
        * Similar to getBlocks as defined above but only retrieves one block at a time. Waits for
        * at most 10 seconds and returns an empty block on failure.
        *
        * @param id the id of the block to retrieve
        * @return the block asked for or an empty block if the function times out
        */
        CryptoKernel::Blockchain::block getBlock(const std::string id);

    private:
        class Peer
        {
            public:
                Peer(sf::TcpSocket* socket, CryptoKernel::Blockchain* blockchain);
                ~Peer();
                bool isConnected();
                CryptoKernel::Blockchain::block getBlock(const std::string id);
                std::vector<CryptoKernel::Blockchain::block> getBlocks(const std::string id);
                void sendBlock(const CryptoKernel::Blockchain::block block);
                void sendTransaction(const CryptoKernel::Blockchain::transaction tx);
                void setMainChain(const bool flag);
                bool getMainChain();
                void disconnect();
                Json::Value getInfo();

            private:
                bool connected;
                bool mainChain;
                std::unique_ptr<sf::TcpSocket> socket;
                CryptoKernel::Blockchain* blockchain;
                std::unique_ptr<std::thread> eventThread;
                Json::Value sendRecv(const Json::Value data);
                void send(const Json::Value data);
                std::mutex peerLock;
                void handleEvents();
        };
        Log* log;
        Blockchain* blockchain;
        void checkRep();
        std::unique_ptr<sf::TcpListener> listener;
        void handleConnections();
        std::vector<Peer*> peers;
        std::unique_ptr<std::thread> connectionsThread;
        bool running;
        std::default_random_engine generator;
        std::map<std::string, uint64_t> nodes;
};
}

#endif // NETWORK_H_INCLUDED
