#include "multicoin.h"

#include "consensus/PoW.h"

CryptoKernel::MulticoinLoader::MulticoinLoader(const std::string& configFile,
                                               Log* log,
                                               bool* running) {
    this->log = log;

    std::ifstream t(configFile);
    if(!t.is_open()) {
        throw std::runtime_error("Could not open multicoin config file");
    }

    const std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    const Json::Value config = CryptoKernel::Storage::toJson(buffer);

    t.close();

    for(const auto& coin : config["coins"]) {
        Coin* newCoin = new Coin;
        newCoin->name = coin["name"].asString();

        auto subsidyFunc = getSubsidyFunc(coin["subsidy"].asString());
        auto coinbaseOwnerFunc = [](const std::string& publicKey) {
                                      return publicKey;
                                  };

        newCoin->blockchain.reset(new DynamicBlockchain(log,
                                                        coin["blockdb"].asString(),
                                                        coinbaseOwnerFunc,
                                                        subsidyFunc));

        newCoin->consensusAlgo = getConsensusAlgo(coin["consensus"]["type"].asString(),
                                                  coin["consensus"]["params"],
                                                  config,
                                                  newCoin->blockchain.get());

        newCoin->blockchain->loadChain(newCoin->consensusAlgo.get(),
                                      coin["genesisblock"].asString());

        newCoin->consensusAlgo->start();

        newCoin->network.reset(new Network(log, newCoin->blockchain.get(),
                                           coin["port"].asUInt(),
                                           coin["peerdb"].asString()));

        if(!coin["walletdb"].empty()) {
            newCoin->wallet.reset(new Wallet(newCoin->blockchain.get(),
                                            newCoin->network.get(),
                                            log,
                                            coin["walletdb"].asString()));
        }

        newCoin->httpserver.reset(new jsonrpc::HttpServerLocal(coin["rpcport"].asUInt(),
                                  config["rpcuser"].asString(),
                                  config["rpcpassword"].asString(),
                                  config["sslcert"].asString(),
                                  config["sslkey"].asString()));
        newCoin->rpcserver.reset(new CryptoServer(*newCoin->httpserver));
        newCoin->rpcserver->setWallet(newCoin->wallet.get(), newCoin->blockchain.get(),
                                      newCoin->network.get(), running);
        newCoin->rpcserver->StartListening();

        coins.push_back(std::unique_ptr<Coin>(newCoin));
    }
}

CryptoKernel::MulticoinLoader::~MulticoinLoader() {
    for(auto& coin : coins) {
        coin->rpcserver->StopListening();
        coin->wallet.reset();
        coin->network.reset();
        coin->consensusAlgo.reset();
    }
}

std::function<uint64_t(const uint64_t)> CryptoKernel::MulticoinLoader::getSubsidyFunc(
                                  const std::string& name) const {
    if(name == "k320") {
        return [](const uint64_t height) {
            const uint64_t COIN = 100000000;
            const uint64_t G = 100 * COIN;
            const uint64_t blocksPerYear = 210240;
            const long double k = 1 + (std::log(1 + 0.032) / blocksPerYear);
            const long double r = 1 + (std::log(1 - 0.24) / blocksPerYear);
            const uint64_t supplyAtSwitchover = 68720300 * COIN;
            const uint64_t switchoverBlock = 1741620;

            if(height > switchoverBlock) {
                const uint64_t supply = supplyAtSwitchover * std::pow(k, height - switchoverBlock);
                return supply * (k - 1);
            } else {
                return G * std::pow(r, height);
            }
        };
    } else {
        throw std::runtime_error("Unknown subsidy function " + name);
    }
}

std::unique_ptr<CryptoKernel::Consensus> CryptoKernel::MulticoinLoader::getConsensusAlgo(
                                         const std::string& name,
                                         const Json::Value& params,
                                         const Json::Value& config,
                                         Blockchain* blockchain) {
    if(name == "kgw_lyra2rev2") {
        return std::unique_ptr<CryptoKernel::Consensus>(
               new Consensus::PoW::KGW_LYRA2REV2(params["blocktime"].asUInt64(),
                                                 blockchain,
                                                 config["miner"].asBool(),
                                                 config["pubKey"].asString(),
                                                 log));
    } else {
        throw std::runtime_error("Unknown consensus algorithm " + name);
    }
}

CryptoKernel::MulticoinLoader::
DynamicBlockchain::DynamicBlockchain(Log* GlobalLog,
                                     const std::string& dbDir,
                                     std::function<std::string(const std::string&)> getCoinbaseOwnerFunc,
                                     std::function<uint64_t(const uint64_t)> getBlockRewardFunc) :
CryptoKernel::Blockchain(GlobalLog, dbDir) {
    this->getCoinbaseOwnerFunc = getCoinbaseOwnerFunc;
    this->getBlockRewardFunc = getBlockRewardFunc;
}

std::string CryptoKernel::MulticoinLoader::
DynamicBlockchain::getCoinbaseOwner(const std::string& publicKey) {
    return getCoinbaseOwnerFunc(publicKey);
}

uint64_t CryptoKernel::MulticoinLoader::
DynamicBlockchain::getBlockReward(const uint64_t height) {
    return getBlockRewardFunc(height);
}
