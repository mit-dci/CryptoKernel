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
#include "consensus/PoW.h"

void miner(CryptoKernel::Blockchain* blockchain, CryptoKernel::Consensus::PoW* consensus, CryptoCurrency::Wallet* wallet, CryptoKernel::Log* log, CryptoKernel::Network* network)
{
    CryptoKernel::Blockchain::block Block;
    wallet->newAddress("mining");

    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);

    while(true)
    {
        if(network->getConnections() > 0 && network->syncProgress() >= 1)
        {
            Block = blockchain->generateVerifyingBlock(wallet->getAddressByName("mining").publicKey);
            uint64_t nonce = 0;

            t = std::time(0);
            now = static_cast<uint64_t> (t);

            uint64_t time2 = now;
            uint64_t count = 0;

            do
            {
                t = std::time(0);
                time2 = static_cast<uint64_t> (t);
                if((time2 - now) % 120 == 0 && (time2 - now) > 0)
                {
                    std::stringstream message;
                    message << "miner(): Hashrate: " << ((count / (time2 - now)) / 1000.0f) << " kH/s";
                    log->printf(LOG_LEVEL_INFO, message.str());
                    Block = blockchain->generateVerifyingBlock(wallet->getAddressByName("mining").publicKey);
                    now = time2;
                    count = 0;
                }

                count += 1;
                nonce += 1;
                Block.consensusData["nonce"] = static_cast<unsigned long long int>(nonce);
                Block.consensusData["PoW"] = consensus->calculatePoW(Block);
            }
            while(!CryptoKernel::Math::hex_greater(Block.consensusData["target"].asString(), Block.consensusData["PoW"].asString()));

            CryptoKernel::Blockchain::block previousBlock;
            previousBlock = blockchain->getBlock(Block.previousBlockId);
            const std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", Block.consensusData["PoW"].asString());
            Block.consensusData["totalWork"] = CryptoKernel::Math::addHex(inverse, previousBlock.consensusData["totalWork"].asString());

            if(blockchain->submitBlock(Block))
            {
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
        MyBlockchain(CryptoKernel::Log* GlobalLog) : CryptoKernel::Blockchain(GlobalLog)
        {

        }

    private:
        uint64_t getBlockReward(const uint64_t height)
        {
            if(height > 2)
            {
                return 100000000 / std::log(height);
            }
            else
            {
                return 100000000;
            }
        }

        std::string getCoinbaseOwner(const std::string publicKey) {
            return publicKey;
        }
};

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        CryptoKernel::Log log("CryptoKernel.log", true);

        MyBlockchain blockchain(&log);
        CryptoKernel::Consensus::PoW::KGW_SHA256 consensus(150, &blockchain);
        blockchain.loadChain(&consensus);
        CryptoKernel::Network network(&log, &blockchain);
        CryptoCurrency::Wallet wallet(&blockchain, &network);
        std::thread minerThread(miner, &blockchain, &consensus, &wallet, &log, &network);

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
                std::cout << "CryptoKernel - Blockchain Development Toolkit - v" << version << "\n\n"
                     << "account [accountname]\n"
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
