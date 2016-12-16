#include <algorithm>

#include "chainsync.h"
#include "ckmath.h"

CryptoKernel::Network::ChainSync::ChainSync(CryptoKernel::Blockchain* blockchain, CryptoKernel::Network* network)
{
    this->blockchain = blockchain;
    this->network = network;
    running = true;

    syncThread.reset(new std::thread(&CryptoKernel::Network::ChainSync::syncLoop, this));

    checkRep();
}

CryptoKernel::Network::ChainSync::~ChainSync()
{
    running = false;
    syncThread->join();
}

void CryptoKernel::Network::ChainSync::checkRep()
{
    assert(blockchain != nullptr);
    assert(network != nullptr);
    assert(syncThread.get() != nullptr);
}

void CryptoKernel::Network::ChainSync::syncLoop()
{
    bool wait = true;
    while(running)
    {
        // Determine best network chain
        // Ask for several block tips
        // Determine which tip has the highest total work
        CryptoKernel::Blockchain::block bestBlock = blockchain->getBlock("tip");
        for(unsigned int i = 0; i < network->getConnections(); i++)
        {
            CryptoKernel::Blockchain::block tip;
            try
            {
                tip = network->getBlock("tip");
            }
            catch(CryptoKernel::Network::NotFoundException& e)
            {

            }

            if(tip.id != "" && CryptoKernel::Math::hex_greater(tip.totalWork, bestBlock.totalWork))
            {
                bestBlock = tip;
            }
        }

        // Check if we are behind best network chain
        const CryptoKernel::Blockchain::block tip = blockchain->getBlock("tip");
        if(CryptoKernel::Math::hex_greater(bestBlock.totalWork, tip.totalWork))
        {
            //If we are, download the missing blocks
            // Ask for our current block tip minus 250 blocks or genesis block
            std::vector<CryptoKernel::Blockchain::block> blocks;
            try
            {
                blocks = network->getBlocksByHeight(std::max(tip.height - 250, static_cast<unsigned int>(1)));
            }
            catch(CryptoKernel::Network::NotFoundException e)
            {
                continue;
            }

            // Check for forks against our chain
            for(const CryptoKernel::Blockchain::block block : blocks)
            {
                // Check if block already exists
                const CryptoKernel::Blockchain::block blockOurs = blockchain->getBlock(block.id);
                if(block.id != blockOurs.id)
                {
                    // If not, check if the previous block exists and submit it
                    if(blockchain->getBlock(block.previousBlockId).id == block.previousBlockId)
                    {
                        if(!blockchain->submitBlock(block))
                        {
                            break;
                        }
                        else
                        {
                            wait = false;
                        }
                    }
                    else
                    {
                        // Otherwise break
                        break;
                    }
                }
            }
        }
        else
        {
            //Else, rebroadcast our unconfirmedTransactions and block tip
            network->sendBlock(tip);
            const std::vector<CryptoKernel::Blockchain::transaction> unconfirmedTransactions = blockchain->getUnconfirmedTransactions();
            for(const CryptoKernel::Blockchain::transaction tx : unconfirmedTransactions)
            {
                network->sendTransaction(tx);
            }
        }
        if(wait)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * 1000));
        }
    }
}
