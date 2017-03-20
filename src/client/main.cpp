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

#include <thread>
#include <iostream>
#include <stdlib.h>
#include <algorithm>

#include <jsonrpccpp/server/connectors/httpserver.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

#include "ckmath.h"
#include "crypto.h"
#include "wallet.h"
#include "network.h"
#include "cryptoserver.h"
#include "cryptoclient.h"
#include "version.h"

void miner(CryptoKernel::Blockchain* blockchain, CryptoCurrency::Wallet* wallet, CryptoKernel::Log* log, CryptoKernel::Network* network)
{
    const CryptoCurrency::Wallet::address verifierAddr = wallet->getAddressByName("jlovejoy@mit.edu");
    uint64_t currentSequenceNumber = blockchain->getBlock("tip").sequenceNumber;
    while(true)
    {
        CryptoKernel::Blockchain::block Block = blockchain->generateMiningBlock(verifierAddr.publicKey);
        if(/*network->getConnections() > 0 &&*/ blockchain->getVerifier(Block) == verifierAddr.publicKey && Block.sequenceNumber > currentSequenceNumber)
        {
            CryptoKernel::Crypto crypto;
            crypto.setPrivateKey(verifierAddr.privateKey);
            Block.signature = crypto.sign(Block.id);

            if(blockchain->submitBlock(Block))
            {
                currentSequenceNumber = Block.sequenceNumber;
                network->broadcastBlock(Block);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

class MyBlockchain : public CryptoKernel::Blockchain
{
    public:
        MyBlockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime) : CryptoKernel::Blockchain(GlobalLog, blockTime)
        {

        }

    private:
        uint64_t getBlockReward(const uint64_t height)
        {
            if(height > 2)
            {
                return 5000000000 / std::log(height);
            }
            else
            {
                return 5000000000;
            }
        }

        std::string PoWFunction(const std::string inputString)
        {
            CryptoKernel::Crypto crypto;
            return crypto.sha256(inputString);
        }
};

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        CryptoKernel::Log log("CryptoKernel.log", true);
        MyBlockchain blockchain(&log, 150);
        blockchain.loadChain();
        CryptoKernel::Network network(&log, &blockchain);
        CryptoCurrency::Wallet wallet(&blockchain, &network);
        std::thread minerThread(miner, &blockchain, &wallet, &log, &network);

        jsonrpc::HttpServer httpserver(8383);
        CryptoServer server(httpserver);
        server.setWallet(&wallet, &blockchain, &network);
        server.StartListening();

        while(true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(120000));
        }

        server.StopListening();
    }
    else
    {
        std::string command(argv[1]);
        jsonrpc::HttpClient httpclient("http://localhost:8383");
        CryptoClient client(httpclient);

        try
        {
            if(command == "getinfo")
            {
                std::cout << CryptoKernel::Storage::toString(client.getinfo()) << std::endl;
            }
            else if(command == "account")
            {
                if(argc == 3)
                {
                    std::string name(argv[2]);
                    std::cout << CryptoKernel::Storage::toString(client.account(name)) << std::endl;
                }
                else
                {
                    std::cout << "Usage: account [accountname]" << std::endl;
                }
            }
            else if(command == "sendtoaddress")
            {
                if(argc == 5)
                {
                    std::string address(argv[2]);
                    double amount(std::strtod(argv[3], NULL));
                    double fee(std::strtod(argv[4], NULL));
                    std::cout << client.sendtoaddress(address, amount, fee) << std::endl;
                }
                else
                {
                    std::cout << "Usage: sendtoaddress [address] [amount] [fee]" << std::endl;
                }
            }
            else if(command == "listaccounts")
            {
                std::cout << CryptoKernel::Storage::toString(client.listaccounts()) << std::endl;
            }
            else if(command == "listunspentoutputs")
            {
                if(argc == 3)
                {
                    std::string name(argv[2]);
                    std::cout << CryptoKernel::Storage::toString(client.listunspentoutputs(name)) << std::endl;
                }
                else
                {
                    std::cout << "Usage: listunspentoutputs [accountname]" << std::endl;
                }
            }
            else if(command == "compilecontract")
            {
                if(argc == 3)
                {
                    std::string code(argv[2]);
                    std::cout << client.compilecontract(code) << std::endl;
                }
                else
                {
                    std::cout << "Usage: compilecontract [code]" << std::endl;
                }
            }
            else if(command == "listtransactions")
            {
                std::cout << CryptoKernel::Storage::toString(client.listtransactions()) << std::endl;
            }
            else
            {
                std::cout << "CryptoKernel - Blockchain Development Toolkit - v" << version << "\n\n";
                std::cout << "account [accountname]\n"
                     << "compilecontract [code]\n"
                     << "getinfo\n"
                     << "listaccounts\n"
                     << "listtransactions\n"
                     << "listunspentoutputs [accountname]\n";
            }
        }
        catch(jsonrpc::JsonRpcException e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
