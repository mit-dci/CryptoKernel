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

#include <cryptokernel/math.h>
#include <cryptokernel/crypto.h>

#include "wallet.h"
#include "cryptoserver.h"
#include "cryptoclient.h"

void miner(CryptoKernel::Blockchain* blockchain, CryptoCurrency::Wallet* wallet, CryptoCurrency::Protocol* protocol, CryptoKernel::Log* log)
{
    CryptoKernel::Blockchain::block Block;
    wallet->newAddress("mining");

    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);

    while(true)
    {
        Block = blockchain->generateMiningBlock(wallet->getAddressByName("mining").publicKey);
        Block.nonce = 0;

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
                uint64_t nonce = Block.nonce;
                Block = blockchain->generateMiningBlock(wallet->getAddressByName("mining").publicKey);
                Block.nonce = nonce;
                now = time2;
                count = 0;
            }

            count += 1;
            Block.nonce += 1;
            Block.PoW = blockchain->calculatePoW(Block);
        }
        while(!CryptoKernel::Math::hex_greater(Block.target, Block.PoW));

        CryptoKernel::Blockchain::block previousBlock;
        previousBlock = blockchain->getBlock(Block.previousBlockId);
        std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", Block.PoW);
        Block.totalWork = CryptoKernel::Math::addHex(inverse, previousBlock.totalWork);

        blockchain->submitBlock(Block);
        protocol->submitBlock(Block);
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
        //CryptoKernel::Blockchain blockchain(&log, 150);
        MyBlockchain blockchain(&log, 150);
        blockchain.loadChain();
        CryptoCurrency::Protocol protocol(&blockchain, &log);
        CryptoCurrency::Wallet wallet(&blockchain, &protocol);
        std::thread minerThread(miner, &blockchain, &wallet, &protocol, &log);

        jsonrpc::HttpServer httpserver(8383);
        CryptoServer server(httpserver);
        server.setWallet(&wallet, &protocol, &blockchain);
        server.StartListening();

        while(true)
        {
            protocol.submitBlock(blockchain.getBlock("tip"));
            std::vector<CryptoKernel::Blockchain::transaction> unconfirmedTransactions = blockchain.getUnconfirmedTransactions();
            std::vector<CryptoKernel::Blockchain::transaction>::iterator it;
            for(it = unconfirmedTransactions.begin(); it < unconfirmedTransactions.end(); it++)
            {
                protocol.submitTransaction(*it);
            }

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
        }
        catch(jsonrpc::JsonRpcException e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
