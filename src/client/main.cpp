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
#include <cmath>
#include <csignal>

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

bool running;

void miner(CryptoKernel::Blockchain* blockchain, CryptoKernel::Consensus::PoW* consensus, CryptoCurrency::Wallet* wallet, CryptoKernel::Log* log, CryptoKernel::Network* network)
{
    wallet->newAddress("mining");

    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);

    while(running)
    {
        if(/*network->getConnections() > 0 &&*/ network->syncProgress() >= 1)
        {
            CryptoKernel::Blockchain::block Block = blockchain->generateVerifyingBlock(wallet->getAddressByName("mining").publicKey);
            uint64_t nonce = 0;

            t = std::time(0);
            now = static_cast<uint64_t> (t);

            uint64_t time2 = now;
            uint64_t count = 0;
            CryptoKernel::BigNum pow;

            CryptoKernel::BigNum target = CryptoKernel::BigNum(Block.getConsensusData()["target"].asString());
            CryptoKernel::Blockchain::dbBlock previousBlock = blockchain->getBlockDB(Block.getPreviousBlockId().toString());
            const CryptoKernel::BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
            Json::Value consensusData = Block.getConsensusData();
            consensusData["totalWork"] = (inverse + CryptoKernel::BigNum(previousBlock.getConsensusData()["totalWork"].asString())).toString();
            consensusData["nonce"] = static_cast<unsigned long long int>(nonce);

            do
            {
                t = std::time(0);
                time2 = static_cast<uint64_t> (t);
                if((time2 - now) % 20 == 0 && (time2 - now) > 0)
                {
                    std::stringstream message;
                    message << "miner(): Hashrate: " << ((count / (time2 - now)) / 1000.0f) << " kH/s";
                    log->printf(LOG_LEVEL_INFO, message.str());
                    Block = blockchain->generateVerifyingBlock(wallet->getAddressByName("mining").publicKey);
                    previousBlock = blockchain->getBlockDB(Block.getPreviousBlockId().toString());
                    target = CryptoKernel::BigNum(Block.getConsensusData()["target"].asString());
                    const CryptoKernel::BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
                    consensusData = Block.getConsensusData();
                    consensusData["totalWork"] = (inverse + CryptoKernel::BigNum(previousBlock.getConsensusData()["totalWork"].asString())).toString();
                    now = time2;
                    count = 0;
                }

                count += 1;
                nonce += 1;

                pow = consensus->calculatePoW(Block, nonce);
            }
            while(pow >= target && running);

            consensusData["nonce"] = static_cast<unsigned long long int>(nonce);
            Block.setConsensusData(consensusData);

            if(running) {
                if(blockchain->submitBlock(Block)) {
                    network->broadcastBlock(Block);
                } else {
                    log->printf(LOG_LEVEL_WARN, "miner(): Produced invalid block! Block: " + Block.toJson().toStyledString() + "\ntarget: " + Block.getConsensusData()["target"].asString() + "\npow: " + consensus->calculatePoW(Block, nonce).toString());
                }
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

        std::string getCoinbaseOwner(const std::string& publicKey) {
            return publicKey;
        }
};

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        CryptoKernel::Log log("CryptoKernel.log", true);

        running = true;
        std::signal(SIGINT, [](int signal){ running = false; });

        MyBlockchain blockchain(&log);
        CryptoKernel::Consensus::PoW::KGW_SHA256 consensus(150, &blockchain);
        blockchain.loadChain(&consensus);
        CryptoKernel::Network network(&log, &blockchain);
        CryptoCurrency::Wallet wallet(&blockchain, &network);
        std::thread minerThread(miner, &blockchain, &consensus, &wallet, &log, &network);

        jsonrpc::HttpServer httpserver(8383, "", "", 1);
        CryptoServer server(httpserver);
        server.setWallet(&wallet, &blockchain, &network, &running);
        server.StartListening();

        while(running)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        log.printf(LOG_LEVEL_INFO, "Shutting down...");

        server.StopListening();
        minerThread.join();
    }
    else
    {
        std::string command(argv[1]);
        jsonrpc::HttpClient httpclient("http://localhost:8383");
        httpclient.SetTimeout(30000);
        CryptoClient client(httpclient);

        try
        {
            if(command == "getinfo")
            {
                std::cout << client.getinfo().toStyledString() << std::endl;
            }
            else if(command == "account")
            {
                if(argc == 3)
                {
                    std::string name(argv[2]);
                    std::cout << client.account(name).toStyledString() << std::endl;
                }
                else
                {
                    std::cout << "Usage: account [accountname]" << std::endl;
                }
            }
            else if(command == "sendtoaddress")
            {
                if(argc == 4)
                {
                    std::string address(argv[2]);
                    double amount(std::strtod(argv[3], NULL));
                    std::cout << client.sendtoaddress(address, amount) << std::endl;
                }
                else
                {
                    std::cout << "Usage: sendtoaddress [address] [amount]" << std::endl;
                }
            }
            else if(command == "listaccounts")
            {
                std::cout << client.listaccounts().toStyledString() << std::endl;
            }
            else if(command == "listunspentoutputs")
            {
                if(argc == 3)
                {
                    std::string name(argv[2]);
                    std::cout << client.listunspentoutputs(name).toStyledString() << std::endl;
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
            else if(command == "listtransactions") {
                std::cout << client.listtransactions().toStyledString() << std::endl;
            } else if(command == "getblockbyheight") {
                if(argc == 3) {
                    std::cout << client.getblockbyheight(std::strtoull(argv[2], nullptr, 0)).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: getblockbyheight [height]" << std::endl;
                }
            } else if(command == "stop") {
                std::cout << client.stop().toStyledString() << std::endl;
            } else {
                std::cout << "CryptoKernel - Blockchain Development Toolkit - v" << version << "\n\n"
                          << "account [accountname]\n"
                          << "compilecontract [code]\n"
                          << "getblockbyheight [height]\n"
                          << "getinfo\n"
                          << "listaccounts\n"
                          << "listtransactions\n"
                          << "listunspentoutputs [accountname]\n"
                          << "sendtoaddress [address] [amount]\n"
                          << "stop\n";
            }
        }
        catch(jsonrpc::JsonRpcException e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
