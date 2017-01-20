#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

namespace CryptoKernel
{
    class Network
    {
        public:
            Network();
            ~Network();

        private:
            class Server;
            class Client;
    };
}

#endif // NETWORK_H_INCLUDED
