#ifndef MULTICOIN_H_INCLUDED
#define MULTICOIN_H_INCLUDED

#include <functional>

#include "blockchain.h"
#include "network.h"
#include "wallet.h"

#include "httpserver.h"
#include "cryptoserver.h"

namespace CryptoKernel {
    class MulticoinLoader {
        public:
            MulticoinLoader(const std::string& configFile,
                            Log* log,
                            bool* running);
            ~MulticoinLoader();

        private:
            struct Coin {
                std::string name;
                std::unique_ptr<Consensus> consensusAlgo;
                std::unique_ptr<Blockchain> blockchain;
                std::unique_ptr<Network> network;
                std::unique_ptr<Wallet> wallet;
                std::unique_ptr<jsonrpc::HttpServerLocal> httpserver;
                std::unique_ptr<CryptoServer> rpcserver;
            };

            std::vector<std::unique_ptr<Coin>> coins;

            Log* log;

            std::function<uint64_t(const uint64_t)> getSubsidyFunc(const std::string& name) const;

            std::unique_ptr<Consensus> getConsensusAlgo(const std::string& name,
                                                        const Json::Value& params,
                                                        const Json::Value& config,
                                                        Blockchain* blockchain);

            class DynamicBlockchain : public Blockchain {
                public:
                    DynamicBlockchain(Log* GlobalLog,
                                      const std::string& dbDir,
                                      std::function<std::string(const std::string&)> getCoinbaseOwnerFunc,
                                      std::function<uint64_t(const uint64_t)> getBlockRewardFunc);

                private:
                    virtual std::string getCoinbaseOwner(const std::string& publicKey);
                    virtual uint64_t getBlockReward(const uint64_t height);

                    std::function<uint64_t(const uint64_t)> getBlockRewardFunc;
                    std::function<std::string(const std::string&)> getCoinbaseOwnerFunc;
            };


    };
}

#endif // MULTICOIN_H_INCLUDED
