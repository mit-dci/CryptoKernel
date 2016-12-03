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

#include <list>
#include <algorithm>
#include <string>

#include "protocol.h"

CryptoCurrency::Protocol::Protocol(CryptoKernel::Blockchain* Blockchain, CryptoKernel::Log* GlobalLog)
{
    blockchain = Blockchain;;
    log = GlobalLog;
    network = new CryptoKernel::Network(log, blockchain);

    eventThread = new std::thread(&CryptoCurrency::Protocol::handleEvent, this);
    rebroadcastThread = new std::thread(&CryptoCurrency::Protocol::rebroadcast, this);
}

CryptoCurrency::Protocol::~Protocol()
{
    delete rebroadcastThread;
    delete eventThread;
    delete network;
    delete log;
}

void CryptoCurrency::Protocol::rebroadcast()
{
    while(true)
    {
        log->printf(LOG_LEVEL_INFO, "protocol::rebroadcast(): Rebroadcasting blocks and unconfirmed transactions. Asking for block tips.");
        Json::Value send;
        send["method"] = "send";
        send["data"] = "tip";

        network->sendMessage(CryptoKernel::Storage::toString(send));

        submitBlock(blockchain->getBlock("tip"));

        std::vector<CryptoKernel::Blockchain::transaction>::iterator it;
        for(it = blockchain->getUnconfirmedTransactions().begin(); it < blockchain->getUnconfirmedTransactions().end(); it++)
        {
            submitTransaction(*it);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(60000));
    }
}

void CryptoCurrency::Protocol::handleEvent()
{
    std::list<std::string> broadcastTransactions;
    CryptoKernel::Storage blockBuffer("./blockbuffer");

    while(true)
    {
        std::string message = network->popMessage();

        if(message == "")
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            Json::Value command = CryptoKernel::Storage::toJson(message);
            if(command["method"].asString() == "block")
            {
                CryptoKernel::Blockchain::block Block = blockchain->jsonToBlock(command["data"]);
                bool blockExists = (blockchain->getBlock(Block.id).id == Block.id);
                if(blockchain->getBlock(Block.previousBlockId).id != Block.previousBlockId)
                {
                    Json::Value send;
                    send["method"] = "send";
                    send["data"] = blockchain->getBlock("tip").id;

                    log->printf(LOG_LEVEL_INFO, std::string("protocol::handleEvent(): Asking for blocks ") + std::to_string(blockchain->getBlock("tip").height + 1) + std::string(" to ") + std::to_string(blockchain->getBlock("tip").height + 201));

                    network->sendMessage(CryptoKernel::Storage::toString(send));
                }
                else if(blockchain->submitBlock(Block) && !blockExists)
                {
                    submitBlock(Block);
                }
            }

            else if(command["method"].asString() == "transaction")
            {
                CryptoKernel::Blockchain::transaction tx = blockchain->jsonToTransaction(command["data"]);
                if(blockchain->submitTransaction(tx) && std::find(broadcastTransactions.begin(), broadcastTransactions.end(), tx.id) != broadcastTransactions.end())
                {
                    broadcastTransactions.push_back(tx.id);
                    submitTransaction(tx);
                }
            }

            else if(command["method"].asString() == "blocks")
            {
                log->printf(LOG_LEVEL_INFO, "protocol::handleEvent(): Received blocks " + command["data"][command["data"].size() - 1]["height"].asString() + " to " + command["data"][0]["height"].asString());

                for(unsigned int i = 0; i < command["data"].size(); i++)
                {
                    blockchain->submitBlock(CryptoKernel::Blockchain::jsonToBlock(command["data"][i]));
                }

                Json::Value send;
                send["method"] = "send";
                send["data"] = blockchain->getBlock("tip").id;

                log->printf(LOG_LEVEL_INFO, std::string("protocol::handleEvent(): Asking for blocks ") + std::to_string(blockchain->getBlock("tip").height + 1) + std::string(" to ") + std::to_string(blockchain->getBlock("tip").height + 201));

                network->sendMessage(CryptoKernel::Storage::toString(send));
            }
            else if(command["method"].asString() == "send")
            {
                std::string baseId = command["data"].asString();

                Json::Value returning;
                returning["method"] = "blocks";

                if(blockchain->getBlock(baseId).id == baseId && baseId != "")
                {
                    CryptoKernel::Blockchain::block Block = blockchain->getBlock(baseId);
                    bool appended = true;
                    for(unsigned int i = 0; i < 200 && appended; i++)
                    {
                        appended = false;
                        CryptoKernel::Storage::Iterator* it = blockBuffer.newIterator();
                        for(it->SeekToFirst(); it->Valid(); it->Next())
                        {
                            if(it->value()["previousBlockId"].asString() == Block.id && Block.id != "")
                            {
                                appended = true;
                                Block.id = it->value()["id"].asString();
                                returning["data"].append(it->value());
                                break;
                            }
                        }
                        delete it;
                    }

                    network->sendMessage(CryptoKernel::Storage::toString(returning));
                }
            }
        }
    }
}

bool CryptoCurrency::Protocol::submitBlock(CryptoKernel::Blockchain::block Block)
{
    Json::Value data;
    data["method"] = "block";
    data["data"] = blockchain->blockToJson(Block);
    if(network->sendMessage(CryptoKernel::Storage::toString(data)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool CryptoCurrency::Protocol::submitTransaction(CryptoKernel::Blockchain::transaction tx)
{
    Json::Value data;
    data["method"] = "transaction";
    data["data"] = blockchain->transactionToJson(tx);
    if(network->sendMessage(CryptoKernel::Storage::toString(data)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

unsigned int CryptoCurrency::Protocol::getConnections()
{
    return network->getConnections();
}
