#ifndef CHAINSYNC_H_INCLUDED
#define CHAINSYNC_H_INCLUDED

#include "network.h"

namespace CryptoKernel
{
    class Network::ChainSync
    {
        public:
            ChainSync(CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network, CryptoKernel::Log* log);
            ~ChainSync();

        private:
            std::unique_ptr<std::thread> syncThread;
            CryptoKernel::Blockchain* blockchain;
            CryptoKernel::Network* network;
            CryptoKernel::Log* log;
            void checkRep();
            void syncLoop();
            bool running;
    };
}

#endif // CHAINSYNC_H_INCLUDED
