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

#include "protocol.h"

CryptoCurrency::Protocol::Protocol(CryptoKernel::Blockchain* Blockchain, CryptoKernel::Log* GlobalLog)
{
    blockchain = Blockchain;;
    log = GlobalLog;
    network = new CryptoKernel::Network(log);

    eventThread = new std::thread(&CryptoCurrency::Protocol::handleEvent, this);
}

CryptoCurrency::Protocol::~Protocol()
{
    delete eventThread;
    delete network;
    delete log;
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
                    send["data"] = Block.id;

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
                for(unsigned int i = 0; i < command["data"].size(); i++)
                {
                    blockBuffer.store(command["data"][i]["id"].asString(), command["data"][i]);
                }

                //Find first block that doesn't exist with a previous block that does exist
                std::string bottomId = "";
                CryptoKernel::Storage::Iterator* it = blockBuffer.newIterator();
                for(it->SeekToFirst(); it->Valid(); it->Next())
                {
                    CryptoKernel::Blockchain::block Block = blockchain->jsonToBlock(it->value());
                    if(Block.id != blockchain->getBlock(Block.id).id && Block.previousBlockId == blockchain->getBlock(Block.previousBlockId).id)
                    {
                        bottomId = Block.id;
                        break;
                    }
                }
                delete it;

                //If found, trace up to the highest block we have adding each one
                if(bottomId != "")
                {
                    bool found = false;
                    do
                    {
                        found = false;
                        CryptoKernel::Blockchain::block Block = blockchain->jsonToBlock(blockBuffer.get(bottomId));
                        if(!blockchain->submitBlock(Block))
                        {
                            found = false;
                        }
                        else
                        {
                            blockBuffer.erase(Block.id);
                            it = blockBuffer.newIterator();
                            for(it->SeekToFirst(); it->Valid(); it->Next())
                            {
                                CryptoKernel::Blockchain::block nextBlock = blockchain->jsonToBlock(it->value());
                                if(nextBlock.previousBlockId == Block.id)
                                {
                                    bottomId = nextBlock.id;
                                    found = true;
                                    break;
                                }
                            }
                            delete it;
                        }
                    }
                    while(found);
                }
                else
                {
                    //Otherwise, request the next set of missing blocks
                    it = blockBuffer.newIterator();
                    std::string nextBlockId = "";
                    for(it->SeekToFirst(); it->Valid(); it->Next())
                    {
                        if(it->value()["id"] != "")
                        {
                            nextBlockId = it->value()["id"].asString();
                            break;
                        }
                    }
                    delete it;

                    bool found = false;

                    do
                    {
                        found = false;
                        CryptoKernel::Blockchain::block nextBlock = blockchain->jsonToBlock(blockBuffer.get(nextBlockId));
                        if(nextBlock.id == nextBlockId && nextBlock.id != "")
                        {
                            found = true;
                            nextBlockId = nextBlock.previousBlockId;
                        }
                    }
                    while(found);

                    if(blockchain->getBlock(nextBlockId).id != nextBlockId)
                    {
                        Json::Value send;
                        send["method"] = "send";
                        send["data"] = nextBlockId;

                        network->sendMessage(CryptoKernel::Storage::toString(send));
                    }
                }
            }
            else if(command["method"].asString() == "send")
            {
                std::string tipId = command["data"].asString();

                Json::Value returning;
                returning["method"] = "blocks";

                if(blockchain->getBlock(tipId).id == tipId && tipId != "")
                {
                    for(unsigned int i = 0; i < 200; i++)
                    {
                        CryptoKernel::Blockchain::block Block = blockchain->getBlock(tipId);
                        returning["data"].append(blockchain->blockToJson(Block));
                        if(Block.previousBlockId == "")
                        {
                            break;
                        }
                        tipId = Block.previousBlockId;
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
