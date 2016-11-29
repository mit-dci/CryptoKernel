/*  CryptoCurrency - An example crypto-currency using the CryptoKernel library as a base
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

#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include <cryptokernel/network.h>
#include <cryptokernel/blockchain.h>

namespace CryptoCurrency
{
    class Protocol
    {
        public:
            Protocol(CryptoKernel::Blockchain* Blockchain, CryptoKernel::Log* GlobalLog);
            ~Protocol();
            bool submitTransaction(CryptoKernel::Blockchain::transaction tx);
            bool submitBlock(CryptoKernel::Blockchain::block Block);
            unsigned int getConnections();

        private:
            CryptoKernel::Network* network;
            CryptoKernel::Blockchain* blockchain;
            CryptoKernel::Log* log;
            void handleEvent();
            std::thread *eventThread;
    };
}

#endif // PROTOCOL_H_INCLUDED
